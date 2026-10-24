#include "dhcp.h"
#include "udp.h"
#include "ipv4.h"
#include "../include/timer.h"
#include "../log/logger.h"

#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u
#define DHCP_PACKET_MIN 300u
#define DHCP_PACKET_MAX 576u
#define DHCP_FIXED_LENGTH 240u
#define DHCP_COOKIE_OFFSET 236u
#define DHCP_TIMEOUT_TICKS 800u
#define DHCP_RETRY_TICKS 200u
#define DHCP_MAX_POLLS 50000000u
#define DHCP_TICKS_PER_SECOND 100u
#define DHCP_REBIND_NUMERATOR 7u
#define DHCP_REBIND_DENOMINATOR 8u

#define DHCP_DISCOVER 1u
#define DHCP_OFFER    2u
#define DHCP_REQUEST  3u
#define DHCP_ACK      5u
#define DHCP_NAK      6u

static dhcp_state_t current_dhcp_state = DHCP_STATE_INIT;
static netdev_t* active_device;
static uint32_t active_xid;
static uint32_t offered_ip;
static uint32_t server_ip;
static uint32_t offered_mask;
static uint32_t offered_gateway;
static uint32_t offered_dns;
static uint32_t lease_seconds;
static uint64_t lease_acquired_at;
static uint64_t lease_renew_at;
static uint64_t lease_rebind_at;
static uint64_t lease_expiry_at;
static uint64_t last_renew_request_at;
static uint8_t client_mac[ETH_ALEN];
static int listener_ready;

static uint32_t read_be32(const uint8_t* p) {
    return ip_to_uint32(p[0], p[1], p[2], p[3]);
}

static uint32_t read_be32_number(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static int valid_unicast_ip(uint32_t ip) {
    uint8_t first = (uint8_t)ip;
    return ip != 0 && ip != UINT32_MAX && first != 0 &&
           first != 127 && first < 224;
}

static int mac_is_usable(const uint8_t mac[ETH_ALEN]) {
    int all_zero = 1, all_ones = 1;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) {
        if (mac[i] != 0) all_zero = 0;
        if (mac[i] != 0xFFu) all_ones = 0;
    }
    return !all_zero && !all_ones && (mac[0] & 1u) == 0;
}

static uint32_t add_option(uint8_t* packet, uint32_t offset,
                           uint8_t code, const uint8_t* value, uint8_t length) {
    packet[offset++] = code;
    packet[offset++] = length;
    for (uint32_t i = 0; i < length; ++i) packet[offset++] = value[i];
    return offset;
}

static int lease_is_active_state(dhcp_state_t state) {
    return state == DHCP_STATE_BOUND || state == DHCP_STATE_RENEWING ||
           state == DHCP_STATE_REBINDING;
}

static uint32_t next_transaction_id(void) {
    uint32_t id = (uint32_t)timer_get_ticks() ^ 0x50484F58u;
    for (uint32_t i = 0; i < ETH_ALEN; ++i)
        id = (id << 5) ^ (id >> 2) ^ client_mac[i];
    return id ? id : 0x50484F58u;
}

static void clear_lease_configuration(void) {
    if (active_device) {
        active_device->ip_addr = 0;
        active_device->netmask = 0;
        active_device->gateway = 0;
        active_device->dns_server = 0;
    }
    offered_ip = server_ip = offered_mask = offered_gateway = offered_dns = 0;
    lease_seconds = 0;
    lease_acquired_at = lease_renew_at = lease_rebind_at = lease_expiry_at = 0;
    last_renew_request_at = 0;
}

static uint64_t seconds_to_ticks(uint32_t seconds) {
    if (seconds > UINT32_MAX / DHCP_TICKS_PER_SECOND)
        return (uint64_t)UINT32_MAX;
    return (uint64_t)seconds * DHCP_TICKS_PER_SECOND;
}

static void schedule_lease_deadlines(uint32_t seconds) {
    uint64_t now = timer_get_ticks();
    uint64_t duration = seconds_to_ticks(seconds);
    lease_seconds = seconds;
    lease_acquired_at = now;
    lease_renew_at = now + duration / 2u;
    lease_rebind_at = now + (duration / DHCP_REBIND_DENOMINATOR) * DHCP_REBIND_NUMERATOR;
    lease_expiry_at = now + duration;
    last_renew_request_at = 0;
}

static uint32_t build_message(uint8_t* packet, uint8_t message_type) {
    for (uint32_t i = 0; i < DHCP_PACKET_MAX; ++i) packet[i] = 0;
    packet[0] = 1; /* BOOTREQUEST */
    packet[1] = 1; /* Ethernet */
    packet[2] = ETH_ALEN;
    write_be32(packet + 4, active_xid);

    int renewing = current_dhcp_state == DHCP_STATE_RENEWING;
    int rebinding = current_dhcp_state == DHCP_STATE_REBINDING;
    if (renewing || rebinding) {
        if (!active_device || !active_device->ip_addr) return 0;
        /* ciaddr is the currently bound address, in network byte order. */
        for (uint32_t i = 0; i < 4u; ++i)
            packet[12u + i] = ((const uint8_t*)&active_device->ip_addr)[i];
        packet[10] = rebinding ? 0x80u : 0u;
    } else {
        packet[10] = 0x80u; /* Broadcast reply requested before address assignment. */
    }
    for (uint32_t i = 0; i < ETH_ALEN; ++i) packet[28u + i] = client_mac[i];
    packet[DHCP_COOKIE_OFFSET] = 99;
    packet[DHCP_COOKIE_OFFSET + 1u] = 130;
    packet[DHCP_COOKIE_OFFSET + 2u] = 83;
    packet[DHCP_COOKIE_OFFSET + 3u] = 99;

    uint32_t offset = DHCP_FIXED_LENGTH;
    packet[offset++] = 53; packet[offset++] = 1; packet[offset++] = message_type;
    uint8_t client_id[1u + ETH_ALEN];
    client_id[0] = 1;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) client_id[i + 1u] = client_mac[i];
    offset = add_option(packet, offset, 61, client_id, sizeof(client_id));

    if (message_type == DHCP_DISCOVER || renewing || rebinding) {
        const uint8_t params[] = {1, 3, 6, 51};
        offset = add_option(packet, offset, 55, params, sizeof(params));
    } else {
        uint8_t ip_bytes[4];
        if (!offered_ip || !server_ip) return 0;
        for (uint32_t i = 0; i < 4u; ++i) ip_bytes[i] = ((const uint8_t*)&offered_ip)[i];
        offset = add_option(packet, offset, 50, ip_bytes, sizeof(ip_bytes));
        for (uint32_t i = 0; i < 4u; ++i) ip_bytes[i] = ((const uint8_t*)&server_ip)[i];
        offset = add_option(packet, offset, 54, ip_bytes, sizeof(ip_bytes));
        const uint8_t params[] = {1, 3, 6, 51};
        offset = add_option(packet, offset, 55, params, sizeof(params));
    }
    packet[offset++] = 255;
    if (offset < DHCP_PACKET_MIN) offset = DHCP_PACKET_MIN;
    return offset;
}

static int send_current_message(void) {
    if (!active_device) return -1;
    uint8_t packet[DHCP_PACKET_MAX];
    uint8_t type = current_dhcp_state == DHCP_STATE_DISCOVER ? DHCP_DISCOVER : DHCP_REQUEST;
    uint32_t length = build_message(packet, type);
    if (!length) return -1;
    uint32_t destination_ip = UINT32_MAX;
    if (current_dhcp_state == DHCP_STATE_RENEWING) destination_ip = server_ip;
    int result = udp_send_packet(active_device, destination_ip, DHCP_CLIENT_PORT,
                                 DHCP_SERVER_PORT, packet, length);
    if (result < 0) klog(LOG_WARNING, "DHCP packet transmission failed");
    return result;
}

static int parse_options(const uint8_t* packet, uint32_t length, uint32_t* yiaddr,
                         uint8_t* message_type, uint32_t* sid, uint32_t* mask,
                         uint32_t* gateway, uint32_t* dns, uint32_t* lease) {
    if (length < DHCP_FIXED_LENGTH || length > DHCP_PACKET_MAX) return -1;
    if (packet[DHCP_COOKIE_OFFSET] != 99 || packet[DHCP_COOKIE_OFFSET + 1u] != 130 ||
        packet[DHCP_COOKIE_OFFSET + 2u] != 83 || packet[DHCP_COOKIE_OFFSET + 3u] != 99)
        return -1;
    if (yiaddr) *yiaddr = read_be32(packet + 16);
    uint32_t offset = DHCP_FIXED_LENGTH;
    int saw_end = 0;
    int saw_message_type = 0;
    if (message_type) *message_type = 0;
    if (sid) *sid = 0;
    if (mask) *mask = 0;
    if (gateway) *gateway = 0;
    if (dns) *dns = 0;
    if (lease) *lease = 0;

    while (offset < length) {
        uint8_t code = packet[offset++];
        if (code == 0) continue; /* PAD */
        if (code == 255) { saw_end = 1; break; }
        if (offset >= length) return -1;
        uint8_t option_length = packet[offset++];
        if ((uint32_t)option_length > length - offset) return -1;
        const uint8_t* value = packet + offset;
        switch (code) {
            case 53:
                if (option_length != 1 || saw_message_type) return -1;
                saw_message_type = 1;
                if (message_type) *message_type = value[0];
                break;
            case 54:
                if (option_length != 4) return -1;
                if (sid && !*sid) *sid = read_be32(value);
                break;
            case 1:
                if (option_length != 4) return -1;
                if (mask && !*mask) *mask = read_be32(value);
                break;
            case 3:
                if (option_length < 4 || (option_length % 4) != 0) return -1;
                if (gateway && !*gateway) *gateway = read_be32(value);
                break;
            case 6:
                if (option_length < 4 || (option_length % 4) != 0) return -1;
                if (dns && !*dns) *dns = read_be32(value);
                break;
            case 51:
                if (option_length != 4) return -1;
                if (lease && !*lease) *lease = read_be32_number(value);
                break;
            default:
                break;
        }
        offset += option_length;
    }
    return saw_end ? 0 : -1;
}

static int dhcp_receive(netdev_t* dev, uint32_t source_ip, uint16_t source_port,
                        uint16_t destination_port, const uint8_t* payload,
                        uint32_t payload_length, void* context) {
    (void)context;
    if (!dev || dev != active_device || !payload ||
        source_port != DHCP_SERVER_PORT || destination_port != DHCP_CLIENT_PORT ||
        payload_length < DHCP_FIXED_LENGTH || payload_length > DHCP_PACKET_MAX)
        return -1;
    if (payload[0] != 2 || payload[1] != 1 || payload[2] != ETH_ALEN) return -1;
    if (read_be32_number(payload + 4) != active_xid) return -1;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) {
        if (payload[28u + i] != client_mac[i]) return -1;
    }

    uint32_t yiaddr = 0, sid = 0, mask = 0, gateway = 0, dns = 0, lease = 0;
    uint8_t message_type = 0;
    if (parse_options(payload, payload_length, &yiaddr, &message_type,
                      &sid, &mask, &gateway, &dns, &lease) != 0)
        return -1;

    if (message_type == DHCP_OFFER && current_dhcp_state == DHCP_STATE_DISCOVER) {
        if (!valid_unicast_ip(yiaddr) || !valid_unicast_ip(sid)) return -1;
        offered_ip = yiaddr;
        server_ip = sid;
        if (mask) offered_mask = mask;
        if (gateway) offered_gateway = gateway;
        if (dns) offered_dns = dns;
        if (lease) lease_seconds = lease;
        /* Transition before building the packet so the sender emits DHCPREQUEST, not DISCOVER. */
        current_dhcp_state = DHCP_STATE_REQUEST;
        int result = send_current_message();
        if (result < 0) {
            current_dhcp_state = DHCP_STATE_FAILED;
            return result;
        }
        klog(LOG_INFO, "DHCP offer validated; requested address and server from the offer");
        return 0;
    }

    if (message_type == DHCP_NAK && current_dhcp_state == DHCP_STATE_REQUEST) {
        if (sid ? sid != server_ip : source_ip != server_ip) return -1;
        current_dhcp_state = DHCP_STATE_FAILED;
        klog(LOG_WARNING, "DHCP server rejected the requested lease");
        return 0;
    }

    if (message_type == DHCP_ACK && current_dhcp_state == DHCP_STATE_REQUEST) {
        if (sid ? sid != server_ip : source_ip != server_ip) return -1;
        if (!yiaddr) yiaddr = offered_ip;
        if (!valid_unicast_ip(yiaddr) || yiaddr != offered_ip) return -1;
        if (mask) offered_mask = mask;
        if (gateway) offered_gateway = gateway;
        if (dns) offered_dns = dns;
        if (lease) lease_seconds = lease;
        if (!offered_mask) return -1;
        if (offered_gateway &&
            (offered_gateway & offered_mask) != (offered_ip & offered_mask)) return -1;
        dev->ip_addr = offered_ip;
        dev->netmask = offered_mask;
        dev->gateway = offered_gateway;
        dev->dns_server = offered_dns;
        current_dhcp_state = DHCP_STATE_BOUND;
        klog(LOG_INFO, "DHCP ACK validated; address configuration applied to active interface");
        return 0;
    }

    (void)source_ip;
    return -1;
}

void dhcp_init(void) {
    current_dhcp_state = DHCP_STATE_INIT;
    active_device = 0;
    active_xid = 0;
    offered_ip = server_ip = offered_mask = offered_gateway = offered_dns = lease_seconds = 0;
    listener_ready = (udp_register_handler(DHCP_CLIENT_PORT, dhcp_receive, 0) == 0);
    klog(listener_ready ? LOG_INFO : LOG_ERROR,
         listener_ready ? "DHCP client initialized with bounded OFFER/ACK validation"
                        : "DHCP client could not register UDP port 68");
}

int dhcp_request_lease(netdev_t* dev) {
    if (!dev || !dev->send_packet || !mac_is_usable(dev->mac_addr) || !listener_ready) {
        klog(LOG_WARNING, "DHCP request refused: interface, MAC address, or UDP listener is unavailable");
        return -1;
    }
    if (current_dhcp_state == DHCP_STATE_BOUND && active_device == dev &&
        dev->ip_addr && dev->netmask) return 0;

    active_device = dev;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) client_mac[i] = dev->mac_addr[i];
    active_xid = (uint32_t)timer_get_ticks() ^ 0x50484F58u;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) active_xid = (active_xid << 5) ^ (active_xid >> 2) ^ client_mac[i];
    if (!active_xid) active_xid = 0x50484F58u;
    offered_ip = server_ip = offered_mask = offered_gateway = offered_dns = lease_seconds = 0;
    current_dhcp_state = DHCP_STATE_DISCOVER;

    if (send_current_message() < 0) {
        current_dhcp_state = DHCP_STATE_FAILED;
        return -2;
    }

    uint64_t phase_start = timer_get_ticks();
    uint64_t last_send = phase_start;
    dhcp_state_t observed_state = current_dhcp_state;
    uint32_t attempts = 1;
    uint32_t polls = 0;

    while (current_dhcp_state != DHCP_STATE_BOUND &&
           current_dhcp_state != DHCP_STATE_FAILED) {
        net_poll();
        if (++polls >= DHCP_MAX_POLLS) break;
        uint64_t now = timer_get_ticks();
        if (current_dhcp_state != observed_state) {
            observed_state = current_dhcp_state;
            phase_start = now;
            last_send = now;
            attempts = 1; /* The OFFER handler already transmitted REQUEST. */
        }
        if (now - phase_start >= DHCP_TIMEOUT_TICKS) break;
        if (now - last_send >= DHCP_RETRY_TICKS && attempts < 4u) {
            if (send_current_message() < 0) {
                current_dhcp_state = DHCP_STATE_FAILED;
                break;
            }
            ++attempts;
            last_send = now;
        }
    }

    if (current_dhcp_state != DHCP_STATE_BOUND) {
        current_dhcp_state = DHCP_STATE_FAILED;
        klog(LOG_WARNING, "DHCP timed out or failed; no lease was applied without a validated ACK");
        return -2;
    }
    return 0;
}

dhcp_state_t dhcp_get_state(void) { return current_dhcp_state; }

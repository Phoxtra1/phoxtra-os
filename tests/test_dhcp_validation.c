#include "../kernel/net/dhcp.h"
#include "../kernel/net/udp.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

static udp_receive_handler_t registered_handler;
static void* registered_context;
static uint16_t registered_port;
static netdev_t* active_dev;
static uint8_t last_outgoing[576];
static uint32_t last_outgoing_length;
static uint32_t last_destination_ip;
static uint16_t last_source_port, last_destination_port;
static uint8_t last_message_type;
static uint64_t fake_ticks;
static unsigned poll_count;
static int response_mode;
static unsigned bad_xid_sent;
static unsigned offer_sent;
static unsigned ack_or_nak_sent;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint64_t timer_get_ticks(void) { return fake_ticks++; }
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (!port || !handler || registered_handler) return -1;
    registered_port = port; registered_handler = handler; registered_context = context;
    return 0;
}
int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (port != registered_port || handler != registered_handler || context != registered_context) return -1;
    registered_port = 0; registered_handler = 0; registered_context = 0;
    return 0;
}
int udp_send_packet(netdev_t* dev, uint32_t destination_ip, uint16_t source_port,
                    uint16_t destination_port, const uint8_t* payload, uint32_t length) {
    if (!dev || !payload || length > sizeof(last_outgoing)) return -1;
    active_dev = dev; last_destination_ip = destination_ip;
    last_source_port = source_port; last_destination_port = destination_port;
    last_outgoing_length = length;
    memcpy(last_outgoing, payload, length);
    uint32_t offset = 240;
    while (offset < length) {
        uint8_t code = last_outgoing[offset++];
        if (code == 0) continue;
        if (code == 255 || offset >= length) break;
        uint8_t option_length = last_outgoing[offset++];
        if (code == 53 && option_length == 1) last_message_type = last_outgoing[offset];
        if ((uint32_t)option_length > length - offset) break;
        offset += option_length;
    }
    return 0;
}
void net_poll(void);

static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ip_to_uint32(a, b, c, d);
}
static int outgoing_has_option(uint8_t wanted) {
    uint32_t offset = 240u;
    while (offset < last_outgoing_length) {
        uint8_t code = last_outgoing[offset++];
        if (code == 255u) return 0;
        if (code == 0u) continue;
        if (offset >= last_outgoing_length) return 0;
        uint8_t length = last_outgoing[offset++];
        if ((uint32_t)length > last_outgoing_length - offset) return 0;
        if (code == wanted) return 1;
        offset += length;
    }
    return 0;
}
static void put_option(uint8_t* p, uint32_t* offset, uint8_t code,
                       const uint8_t* value, uint8_t length) {
    p[(*offset)++] = code; p[(*offset)++] = length;
    memcpy(p + *offset, value, length); *offset += length;
}
static uint32_t make_reply(uint8_t* p, uint8_t type, int corrupt_xid) {
    memset(p, 0, 300);
    p[0] = 2; p[1] = 1; p[2] = 6;
    memcpy(p + 4, last_outgoing + 4, 4);
    if (corrupt_xid) p[7] ^= 0x40;
    const uint8_t yiaddr[4] = {192,168,1,105};
    int renewal_packet = last_outgoing[12] || last_outgoing[13] ||
                        last_outgoing[14] || last_outgoing[15];
    if (!renewal_packet) memcpy(p + 16, yiaddr, sizeof(yiaddr));
    memcpy(p + 28, last_outgoing + 28, 6);
    p[236] = 99; p[237] = 130; p[238] = 83; p[239] = 99;
    uint32_t o = 240;
    const uint8_t mt[] = {type};
    const uint8_t sid[] = {192,168,1,1};
    const uint8_t mask[] = {255,255,255,0};
    const uint8_t router[] = {192,168,1,1};
    const uint8_t dns[] = {8,8,8,8};
    const uint8_t lease[] = {0,0,0,8};
    put_option(p, &o, 53, mt, sizeof(mt));
    put_option(p, &o, 54, sid, sizeof(sid));
    if (type == 2 || type == 5) {
        put_option(p, &o, 1, mask, sizeof(mask));
        put_option(p, &o, 3, router, sizeof(router));
        put_option(p, &o, 6, dns, sizeof(dns));
        put_option(p, &o, 51, lease, sizeof(lease));
    }
    p[o++] = 255;
    return 300;
}
void net_poll(void) {
    ++poll_count;
    if (!registered_handler || !active_dev) return;
    uint8_t response[300];
    if (last_message_type == 1 && !offer_sent) {
        if (!bad_xid_sent) {
            uint32_t n = make_reply(response, 2, 1);
            (void)registered_handler(active_dev, ip4(192,168,1,1), 67,
                                    registered_port, response, n, registered_context);
            bad_xid_sent = 1;
        }
        uint32_t n = make_reply(response, 2, 0);
        (void)registered_handler(active_dev, ip4(192,168,1,1), 67,
                                registered_port, response, n, registered_context);
        offer_sent = 1;
    } else if (last_message_type == 3 && !ack_or_nak_sent) {
        uint8_t message_type = response_mode == 1 ? 6 : 5;
        uint32_t n = make_reply(response, message_type, 0);
        (void)registered_handler(active_dev, ip4(192,168,1,1), 67,
                                registered_port, response, n, registered_context);
        ack_or_nak_sent = 1;
    }
}
#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

static void reset_harness(void) {
    registered_handler = 0; registered_context = 0; registered_port = 0;
    active_dev = 0; last_outgoing_length = 0; last_destination_ip = 0;
    last_source_port = last_destination_port = 0; last_message_type = 0;
    fake_ticks = 0; poll_count = 0; bad_xid_sent = offer_sent = ack_or_nak_sent = 0;
}
static int fake_send(netdev_t* dev, const uint8_t* data, uint32_t length) {
    (void)dev; (void)data; (void)length; return 0;
}
static void init_dev(netdev_t* dev) {
    memset(dev, 0, sizeof(*dev));
    const uint8_t mac[6] = {0x02,0x10,0x20,0x30,0x40,0x50};
    memcpy(dev->mac_addr, mac, sizeof(mac));
    dev->send_packet = fake_send;
}

int main(void) {
    netdev_t dev;
    reset_harness(); init_dev(&dev);
    dhcp_init();
    CHECK("DHCP registers UDP client port 68", registered_port == 68 && registered_handler != 0);
    CHECK("DHCP DISCOVER obtains a validated OFFER and ACK",
          dhcp_request_lease(&dev) == 0 && dhcp_get_state() == DHCP_STATE_BOUND);
    CHECK("DISCOVER and REQUEST use DHCP ports and broadcast destination",
          last_destination_ip == UINT32_MAX && last_source_port == 68 && last_destination_port == 67);
    CHECK("validated lease applies address, subnet, gateway and DNS",
          dev.ip_addr == ip4(192,168,1,105) &&
          dev.netmask == ip4(255,255,255,0) &&
          dev.gateway == ip4(192,168,1,1) &&
          dev.dns_server == ip4(8,8,8,8));
    CHECK("invalid transaction ID was rejected before the valid OFFER",
          bad_xid_sent == 1 && offer_sent == 1 && ack_or_nak_sent == 1);
    CHECK("repeated call on an active bound interface is idempotent",
          dhcp_request_lease(&dev) == 0 && dhcp_get_state() == DHCP_STATE_BOUND);
    CHECK("lease duration is read from DHCP option 51", dhcp_get_lease_seconds() == 8u);

    /* Move fake time to T1 without waiting for real wall-clock time. */
    uint64_t bound_time = fake_ticks;
    fake_ticks = bound_time + 400u;
    dhcp_poll();
    CHECK("DHCP sends unicast renewal at T1", dhcp_get_state() == DHCP_STATE_RENEWING &&
          last_destination_ip == ip4(192,168,1,1) && last_message_type == 3 &&
          last_outgoing[12] != 0 && last_outgoing[13] == 168);
    CHECK("renewal request uses ciaddr and omits selecting-only options",
          last_outgoing[12] == 192 && last_outgoing[13] == 168 &&
          last_outgoing[14] == 1 && last_outgoing[15] == 105 &&
          !outgoing_has_option(50u) && !outgoing_has_option(54u));
    uint8_t renewal_ack[300];
    uint32_t renewal_ack_length = make_reply(renewal_ack, 5, 0);
    CHECK("validated renewal ACK is accepted",
          registered_handler(active_dev, ip4(192,168,1,1), 67, registered_port,
                             renewal_ack, renewal_ack_length, registered_context) == 0 &&
          dhcp_get_state() == DHCP_STATE_BOUND && dhcp_get_lease_seconds() == 8u);
    CHECK("renewal preserves the assigned address and configuration",
          dev.ip_addr == ip4(192,168,1,105) && dev.netmask == ip4(255,255,255,0) &&
          dev.gateway == ip4(192,168,1,1) && dev.dns_server == ip4(8,8,8,8));
    fake_ticks += 800u;
    dhcp_poll();
    CHECK("expired lease clears stale interface configuration",
          dhcp_get_state() == DHCP_STATE_FAILED && dev.ip_addr == 0 &&
          dev.netmask == 0 && dev.gateway == 0 && dev.dns_server == 0);

    reset_harness(); init_dev(&dev); response_mode = 1; dhcp_init();
    CHECK("DHCP NAK fails the lease without assigning fabricated configuration",
          dhcp_request_lease(&dev) == -2 && dhcp_get_state() == DHCP_STATE_FAILED &&
          dev.ip_addr == 0 && dev.netmask == 0 && dev.gateway == 0 && dev.dns_server == 0);
    printf("DHCP validation tests passed.\n");
    return 0;
}

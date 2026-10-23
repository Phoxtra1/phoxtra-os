#include "arp.h"
#include "ethernet.h"
#include "../include/timer.h"
#include "../log/logger.h"

#define ARP_TABLE_SIZE 32
#define ARP_PENDING_SIZE 16
#define ARP_PENDING_TTL_TICKS 300u /* 3 seconds at the PIT's configured 100 Hz. */

typedef struct arp_pending {
    uint32_t ip;
    uint64_t expires_at;
    uint8_t valid;
} arp_pending_t;

static arp_entry_t arp_table[ARP_TABLE_SIZE];
static arp_pending_t arp_pending[ARP_PENDING_SIZE];

static int mac_equal(const uint8_t a[ETH_ALEN], const uint8_t b[ETH_ALEN]) {
    for (int i = 0; i < ETH_ALEN; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static int valid_unicast_mac(const uint8_t mac[ETH_ALEN]) {
    int all_zero = 1, all_ones = 1;
    for (int i = 0; i < ETH_ALEN; ++i) {
        if (mac[i] != 0) all_zero = 0;
        if (mac[i] != 0xFF) all_ones = 0;
    }
    return !all_zero && !all_ones && (mac[0] & 1u) == 0;
}

/* IP addresses use the kernel's byte-oriented uint32 representation. */
static int valid_unicast_ipv4(uint32_t ip) {
    uint8_t first_octet = (uint8_t)ip;
    return ip != 0 && ip != UINT32_MAX &&
           first_octet != 0 && first_octet != 127 && first_octet < 224;
}

static int same_subnet(uint32_t a, uint32_t b, uint32_t mask) {
    return mask != 0 && (a & mask) == (b & mask);
}

static int tick_deadline_passed(uint64_t now, uint64_t deadline) {
    return (int64_t)(now - deadline) >= 0;
}

static void pending_expire(uint64_t now) {
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) {
        if (arp_pending[i].valid && tick_deadline_passed(now, arp_pending[i].expires_at))
            arp_pending[i].valid = 0;
    }
}

static int pending_contains(uint32_t ip) {
    uint64_t now = timer_get_ticks();
    pending_expire(now);
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) {
        if (arp_pending[i].valid && arp_pending[i].ip == ip) return 1;
    }
    return 0;
}

static int pending_record(uint32_t ip) {
    uint64_t now = timer_get_ticks();
    pending_expire(now);
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) {
        if (arp_pending[i].valid && arp_pending[i].ip == ip) {
            arp_pending[i].expires_at = now + ARP_PENDING_TTL_TICKS;
            return 0;
        }
    }
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) {
        if (!arp_pending[i].valid) {
            arp_pending[i].ip = ip;
            arp_pending[i].expires_at = now + ARP_PENDING_TTL_TICKS;
            arp_pending[i].valid = 1;
            return 0;
        }
    }
    return -1;
}

static void pending_remove(uint32_t ip) {
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) {
        if (arp_pending[i].valid && arp_pending[i].ip == ip) arp_pending[i].valid = 0;
    }
}

void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) arp_table[i].valid = 0;
    for (int i = 0; i < ARP_PENDING_SIZE; ++i) arp_pending[i].valid = 0;
    klog(LOG_WARNING, "ARP cache initialized empty; replies require a matching unexpired request; unsolicited/off-link cache learning is rejected");
}

int arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]) {
    if (!ip || !mac_out) return -1;
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            for (int k = 0; k < ETH_ALEN; ++k) mac_out[k] = arp_table[i].mac[k];
            return 0;
        }
    }
    return -1;
}

void arp_insert(uint32_t ip, const uint8_t mac[ETH_ALEN]) {
    if (!valid_unicast_ipv4(ip) || !mac || !valid_unicast_mac(mac)) return;
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) {
        if (!arp_table[i].valid || arp_table[i].ip == ip) {
            arp_table[i].ip = ip;
            for (int k = 0; k < ETH_ALEN; ++k) arp_table[i].mac[k] = mac[k];
            arp_table[i].valid = 1;
            return;
        }
    }
}

int arp_send_request(netdev_t* dev, uint32_t target_ip) {
    if (!dev || !dev->send_packet || !valid_unicast_ipv4(target_ip) ||
        !valid_unicast_mac(dev->mac_addr) || !valid_unicast_ipv4(dev->ip_addr) ||
        !same_subnet(target_ip, dev->ip_addr, dev->netmask)) return -1;
    if (pending_record(target_ip) != 0) {
        klog(LOG_WARNING, "ARP request blocked: pending lookup table is full");
        return -1;
    }

    arp_header_t arp;
    arp.hw_type = htons(1);
    arp.proto_type = htons(ETHERTYPE_IPV4);
    arp.hw_size = ETH_ALEN;
    arp.proto_size = 4;
    arp.opcode = htons(ARP_OP_REQUEST);
    arp.sender_ip = dev->ip_addr;
    arp.target_ip = target_ip;
    for (int i = 0; i < ETH_ALEN; ++i) {
        arp.sender_mac[i] = dev->mac_addr[i];
        arp.target_mac[i] = 0;
    }
    const uint8_t broadcast[ETH_ALEN] = {255,255,255,255,255,255};
    int result = ethernet_send_frame(dev, broadcast, ETHERTYPE_ARP, (const uint8_t*)&arp, sizeof(arp));
    if (result < 0) pending_remove(target_ip);
    return result;
}

int arp_receive_packet(netdev_t* dev, const uint8_t* payload, uint32_t length) {
    if (!dev || !payload || length < sizeof(arp_header_t)) return -1;
    const arp_header_t* req = (const arp_header_t*)payload;
    if (ntohs(req->hw_type) != 1 || ntohs(req->proto_type) != ETHERTYPE_IPV4 ||
        req->hw_size != ETH_ALEN || req->proto_size != 4) return -1;

    uint16_t op = ntohs(req->opcode);
    if (op != ARP_OP_REQUEST && op != ARP_OP_REPLY) return -1;
    if (!valid_unicast_mac(req->sender_mac) ||
        !valid_unicast_ipv4(req->sender_ip) ||
        !valid_unicast_mac(dev->mac_addr)) return -1;

    /* Ignore our own frames and duplicate-IP claims rather than learning over our identity. */
    if (mac_equal(req->sender_mac, dev->mac_addr) ||
        (dev->ip_addr && req->sender_ip == dev->ip_addr)) return -1;

    int configured = valid_unicast_ipv4(dev->ip_addr) && dev->netmask != 0;
    int on_link_peer = configured && same_subnet(req->sender_ip, dev->ip_addr, dev->netmask);

    if (op == ARP_OP_REPLY) {
        /* Replies may enter cache only if they match a request that has not timed out. */
        if (!configured || !on_link_peer || req->target_ip != dev->ip_addr ||
            !mac_equal(req->target_mac, dev->mac_addr) || !pending_contains(req->sender_ip)) return -1;
        arp_insert(req->sender_ip, req->sender_mac);
        pending_remove(req->sender_ip);
        return 0;
    }

    /* Learn requests only if the sender is on-link and the request is directed to us. */
    if (on_link_peer && req->target_ip == dev->ip_addr) {
        arp_insert(req->sender_ip, req->sender_mac);
        if (dev->send_packet) {
            arp_header_t reply;
            reply.hw_type = htons(1);
            reply.proto_type = htons(ETHERTYPE_IPV4);
            reply.hw_size = ETH_ALEN;
            reply.proto_size = 4;
            reply.opcode = htons(ARP_OP_REPLY);
            reply.sender_ip = dev->ip_addr;
            reply.target_ip = req->sender_ip;
            for (int i = 0; i < ETH_ALEN; ++i) {
                reply.sender_mac[i] = dev->mac_addr[i];
                reply.target_mac[i] = req->sender_mac[i];
            }
            return ethernet_send_frame(dev, req->sender_mac, ETHERTYPE_ARP,
                                       (const uint8_t*)&reply, sizeof(reply));
        }
    }
    return 0;
}

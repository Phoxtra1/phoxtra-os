#include "arp.h"
#include "ethernet.h"
#include "../log/logger.h"

#define ARP_TABLE_SIZE 32

static arp_entry_t arp_table[ARP_TABLE_SIZE];

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

void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) arp_table[i].valid = 0;
    klog(LOG_WARNING, "ARP cache initialized empty; only validated on-link senders and replies addressed to this interface are learned");
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
        !valid_unicast_mac(dev->mac_addr) || !valid_unicast_ipv4(dev->ip_addr)) return -1;
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
    return ethernet_send_frame(dev, broadcast, ETHERTYPE_ARP, (const uint8_t*)&arp, sizeof(arp));
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

    if (op == ARP_OP_REPLY) {
        /* An unsolicited reply must not overwrite the cache. Accept only replies addressed to us. */
        if (!valid_unicast_ipv4(dev->ip_addr) || req->target_ip != dev->ip_addr ||
            !mac_equal(req->target_mac, dev->mac_addr)) return -1;
        arp_insert(req->sender_ip, req->sender_mac);
        return 0;
    }

    /* Learn requests only from a configured on-link peer. */
    int on_link_peer = valid_unicast_ipv4(dev->ip_addr) &&
                       same_subnet(req->sender_ip, dev->ip_addr, dev->netmask);
    if (on_link_peer) arp_insert(req->sender_ip, req->sender_mac);

    if (on_link_peer && req->target_ip == dev->ip_addr && dev->send_packet) {
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
    return 0;
}

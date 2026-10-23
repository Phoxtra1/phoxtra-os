#include "arp.h"
#include "ethernet.h"
#include "../log/logger.h"
#define ARP_TABLE_SIZE 32
static arp_entry_t arp_table[ARP_TABLE_SIZE];
void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) arp_table[i].valid = 0;
    klog(LOG_WARNING, "ARP cache initialized empty; entries are learned from validated packets");
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
    if (!ip || !mac) return;
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
    if (!dev || !dev->send_packet || !target_ip) return -1;
    arp_header_t arp;
    arp.hw_type = htons(1); arp.proto_type = htons(ETHERTYPE_IPV4);
    arp.hw_size = ETH_ALEN; arp.proto_size = 4; arp.opcode = htons(ARP_OP_REQUEST);
    arp.sender_ip = dev->ip_addr; arp.target_ip = target_ip;
    for (int i = 0; i < ETH_ALEN; ++i) { arp.sender_mac[i] = dev->mac_addr[i]; arp.target_mac[i] = 0; }
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
    int sender_mac_valid = 0;
    for (int i = 0; i < ETH_ALEN; ++i) if (req->sender_mac[i] != 0) sender_mac_valid = 1;
    if (!sender_mac_valid || req->sender_ip == 0) return -1;
    arp_insert(req->sender_ip, req->sender_mac);

    if (op == ARP_OP_REQUEST && dev->ip_addr && req->target_ip == dev->ip_addr && dev->send_packet) {
        arp_header_t reply;
        reply.hw_type = htons(1); reply.proto_type = htons(ETHERTYPE_IPV4);
        reply.hw_size = ETH_ALEN; reply.proto_size = 4; reply.opcode = htons(ARP_OP_REPLY);
        reply.sender_ip = dev->ip_addr; reply.target_ip = req->sender_ip;
        for (int i = 0; i < ETH_ALEN; ++i) {
            reply.sender_mac[i] = dev->mac_addr[i];
            reply.target_mac[i] = req->sender_mac[i];
        }
        return ethernet_send_frame(dev, req->sender_mac, ETHERTYPE_ARP,
                                   (const uint8_t*)&reply, sizeof(reply));
    }
    return 0;
}

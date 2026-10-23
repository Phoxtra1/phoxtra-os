#include "arp.h"
#include "ethernet.h"
#include "../log/logger.h"
#define ARP_TABLE_SIZE 32
static arp_entry_t arp_table[ARP_TABLE_SIZE];
void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; ++i) arp_table[i].valid = 0;
    klog(LOG_WARNING, "ARP cache initialized empty; replies are not yet processed by the receive path");
}
int arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]) {
    if (!mac_out) return -1;
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
    return ethernet_send_frame(dev, broadcast, ETHERTYPE_ARP,
                               (const uint8_t*)&arp, sizeof(arp_header_t));
}

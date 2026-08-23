#ifndef PHOXTRA_ARP_H
#define PHOXTRA_ARP_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

typedef struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_size;
    uint8_t  proto_size;
    uint16_t opcode;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

typedef struct arp_entry {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    uint8_t  valid;
} arp_entry_t;

void arp_init(void);
int arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]);
void arp_insert(uint32_t ip, const uint8_t mac[ETH_ALEN]);
int arp_send_request(netdev_t* dev, uint32_t target_ip);

#ifdef __cplusplus
}
#endif

#endif

#ifndef PHOXTRA_ETHERNET_H
#define PHOXTRA_ETHERNET_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD

typedef struct eth_header {
    uint8_t  dest_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

void ethernet_init(void);
int ethernet_send_frame(netdev_t* dev, const uint8_t dest_mac[ETH_ALEN], uint16_t ethertype, const uint8_t* payload, uint32_t payload_len);
int ethernet_receive_frame(netdev_t* dev, const uint8_t* frame, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif

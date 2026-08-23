#ifndef PHOXTRA_IPV4_H
#define PHOXTRA_IPV4_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

typedef struct ipv4_header {
    uint8_t  ihl_version;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t fragment_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

typedef struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

void ipv4_init(void);
uint16_t net_checksum(const void* data, uint32_t len);
int ipv4_send_packet(netdev_t* dev, uint32_t dest_ip, uint8_t protocol, const uint8_t* payload, uint32_t payload_len);
int icmp_ping(netdev_t* dev, uint32_t dest_ip);

#ifdef __cplusplus
}
#endif

#endif

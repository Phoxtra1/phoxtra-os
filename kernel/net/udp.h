#ifndef PHOXTRA_UDP_H
#define PHOXTRA_UDP_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct udp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

void udp_init(void);
int udp_send_packet(netdev_t* dev, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const uint8_t* payload, uint32_t payload_len);

#ifdef __cplusplus
}
#endif

#endif

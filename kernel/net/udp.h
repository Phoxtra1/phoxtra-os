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

typedef int (*udp_receive_handler_t)(netdev_t* dev, uint32_t source_ip,
                                     uint16_t source_port, uint16_t destination_port,
                                     const uint8_t* payload, uint32_t payload_length,
                                     void* context);

void udp_init(void);
int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context);
int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context);
int udp_receive_packet(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                       const uint8_t* packet, uint32_t length);
int udp_send_packet(netdev_t* dev, uint32_t dest_ip, uint16_t src_port,
                    uint16_t dest_port, const uint8_t* payload, uint32_t payload_len);

#ifdef __cplusplus
}
#endif

#endif

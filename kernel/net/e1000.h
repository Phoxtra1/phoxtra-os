#ifndef PHOXTRA_E1000_H
#define PHOXTRA_E1000_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E  /* Intel 82540EM Gigabit Ethernet Controller */

typedef struct e1000_rx_desc {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct e1000_tx_desc {
    uint64_t address;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

void e1000_init(void);
netdev_t* e1000_get_netdev(void);
int e1000_send_packet(netdev_t* dev, const uint8_t* buffer, uint32_t len);
int e1000_receive_packet(netdev_t* dev, uint8_t* buffer, uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif

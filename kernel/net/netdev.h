#ifndef PHOXTRA_NETDEV_H
#define PHOXTRA_NETDEV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NETDEV_COUNT 4
#define ETH_ALEN 6
#define MAX_ETH_FRAME_LEN 1518

typedef struct netdev {
    char name[16];
    uint8_t mac_addr[ETH_ALEN];
    uint32_t ip_addr;     /* IPv4 Network order */
    uint32_t netmask;     /* Subnet mask */
    uint32_t gateway;     /* Default gateway */
    uint32_t dns_server;  /* Primary DNS server */
    uint32_t flags;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;

    int (*send_packet)(struct netdev* dev, const uint8_t* buffer, uint32_t len);
    int (*receive_packet)(struct netdev* dev, uint8_t* buffer, uint32_t max_len);
} netdev_t;

void netdev_init(void);
int netdev_register(netdev_t* dev);
netdev_t* netdev_get_by_index(uint32_t index);
netdev_t* netdev_get_default(void);
uint32_t netdev_count(void);

uint16_t htons(uint16_t val);
uint16_t ntohs(uint16_t val);
uint32_t htonl(uint32_t val);
uint32_t ntohl(uint32_t val);
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void uint32_to_ip(uint32_t ip, char* out);

#ifdef __cplusplus
}
#endif

#endif

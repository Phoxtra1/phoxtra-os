#include "netdev.h"
#include "ethernet.h"
#include "tcp.h"
#include "../log/logger.h"
static netdev_t netdev_list[MAX_NETDEV_COUNT];
static uint32_t registered_netdev_count;
static uint8_t rx_frame[MAX_ETH_FRAME_LEN];
void netdev_init(void) {
    registered_netdev_count = 0;
    klog(LOG_INFO, "Initializing Network Device Abstraction Layer...");
}
int netdev_register(netdev_t* dev) {
    if (!dev || registered_netdev_count >= MAX_NETDEV_COUNT) return -1;
    netdev_list[registered_netdev_count] = *dev;
    klog(LOG_INFO, "Registered hardware network interface");
    ++registered_netdev_count;
    return 0;
}
netdev_t* netdev_get_by_index(uint32_t index) {
    return index < registered_netdev_count ? &netdev_list[index] : 0;
}
netdev_t* netdev_get_default(void) { return registered_netdev_count ? &netdev_list[0] : 0; }
uint32_t netdev_count(void) { return registered_netdev_count; }
void net_poll(void) {
    netdev_t* dev = netdev_get_default();
    if (dev && dev->receive_packet) {
        for (uint32_t i = 0; i < 8; ++i) {
            int length = dev->receive_packet(dev, rx_frame, sizeof(rx_frame));
            if (length <= 0) break;
            if (length <= (int)sizeof(rx_frame))
                (void)ethernet_receive_frame(dev, rx_frame, (uint32_t)length);
        }
    }
    /* Retry half-open passive TCP handshakes even when no frame was queued this poll. */
    tcp_poll();
}
uint16_t htons(uint16_t val) { return (uint16_t)((val << 8) | (val >> 8)); }
uint16_t ntohs(uint16_t val) { return htons(val); }
uint32_t htonl(uint32_t val) {
    return ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) |
           ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24);
}
uint32_t ntohl(uint32_t val) { return htonl(val); }
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)(a | (b << 8) | (c << 16) | (d << 24));
}
void uint32_to_ip(uint32_t ip, char* out) {
    if (!out) return;
    const uint8_t* p = (const uint8_t*)&ip; int pos = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t val = p[i];
        if (val >= 100) { out[pos++]='0'+val/100; val%=100; out[pos++]='0'+val/10; val%=10; }
        else if (val >= 10) { out[pos++]='0'+val/10; val%=10; }
        out[pos++]='0'+val; if (i < 3) out[pos++]='.';
    }
    out[pos]='\0';
}

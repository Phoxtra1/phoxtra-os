#include "ethernet.h"
#include "../log/logger.h"

void ethernet_init(void) {
    klog(LOG_INFO, "Initializing Ethernet II Layer (EtherType 0x0800 IPv4 / 0x0806 ARP)...");
}

int ethernet_send_frame(netdev_t* dev, const uint8_t dest_mac[ETH_ALEN], uint16_t ethertype, const uint8_t* payload, uint32_t payload_len) {
    if (!dev || !payload) return -1;

    uint8_t frame[1518];
    eth_header_t* header = (eth_header_t*)frame;

    for (int i = 0; i < ETH_ALEN; i++) {
        header->dest_mac[i] = dest_mac[i];
        header->src_mac[i]  = dev->mac_addr[i];
    }
    header->ethertype = htons(ethertype);

    for (uint32_t i = 0; i < payload_len; i++) {
        frame[sizeof(eth_header_t) + i] = payload[i];
    }

    uint32_t total_len = sizeof(eth_header_t) + payload_len;
    return dev->send_packet(dev, frame, total_len);
}

int ethernet_receive_frame(netdev_t* dev, const uint8_t* frame, uint32_t len) {
    if (!dev || !frame || len < sizeof(eth_header_t)) return -1;
    dev->rx_packets++;
    dev->rx_bytes += len;
    return 0;
}

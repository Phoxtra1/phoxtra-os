#include "udp.h"
#include "ipv4.h"
#include "../log/logger.h"

void udp_init(void) {
    klog(LOG_INFO, "Initializing User Datagram Protocol (UDP) Layer...");
}

int udp_send_packet(netdev_t* dev, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const uint8_t* payload, uint32_t payload_len) {
    if (!dev || !payload) return -1;

    uint8_t packet[1480];
    udp_header_t* udp = (udp_header_t*)packet;

    udp->src_port  = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length    = htons(sizeof(udp_header_t) + payload_len);
    udp->checksum  = 0; // Checksum optional for IPv4 UDP

    for (uint32_t i = 0; i < payload_len; i++) {
        packet[sizeof(udp_header_t) + i] = payload[i];
    }

    return ipv4_send_packet(dev, dest_ip, IPPROTO_UDP, packet, sizeof(udp_header_t) + payload_len);
}

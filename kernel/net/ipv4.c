#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "../log/logger.h"

static uint16_t packet_id = 1;

void ipv4_init(void) {
    klog(LOG_INFO, "Initializing IPv4 Packet Processing Engine & ICMP Ping Layer...");
}

uint16_t net_checksum(const void* data, uint32_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

int ipv4_send_packet(netdev_t* dev, uint32_t dest_ip, uint8_t protocol, const uint8_t* payload, uint32_t payload_len) {
    if (!dev || !payload) return -1;

    uint8_t packet[1500];
    ipv4_header_t* ip = (ipv4_header_t*)packet;

    ip->ihl_version     = 0x45; // IPv4, 20-byte header
    ip->tos             = 0;
    ip->total_length    = htons(sizeof(ipv4_header_t) + payload_len);
    ip->id              = htons(packet_id++);
    ip->fragment_offset = 0;
    ip->ttl             = 64;
    ip->protocol        = protocol;
    ip->checksum        = 0;
    ip->src_ip          = dev->ip_addr;
    ip->dest_ip         = dest_ip;
    ip->checksum        = net_checksum(ip, sizeof(ipv4_header_t));

    for (uint32_t i = 0; i < payload_len; i++) {
        packet[sizeof(ipv4_header_t) + i] = payload[i];
    }

    // Resolve target MAC address via ARP lookup
    uint8_t target_mac[ETH_ALEN];
    if (arp_lookup(dest_ip, target_mac) != 0) {
        // Fallback to Gateway MAC
        arp_lookup(dev->gateway, target_mac);
    }

    return ethernet_send_frame(dev, target_mac, ETHERTYPE_IPV4, packet, sizeof(ipv4_header_t) + payload_len);
}

int icmp_ping(netdev_t* dev, uint32_t dest_ip) {
    if (!dev) return -1;

    uint8_t payload[64];
    icmp_header_t* icmp = (icmp_header_t*)payload;
    icmp->type     = 8; // ICMP Echo Request
    icmp->code     = 0;
    icmp->checksum = 0;
    icmp->id       = htons(0x1234);
    icmp->sequence = htons(1);

    for (int i = sizeof(icmp_header_t); i < 64; i++) {
        payload[i] = 'A' + (i % 26);
    }
    icmp->checksum = net_checksum(icmp, 64);

    return ipv4_send_packet(dev, dest_ip, IPPROTO_ICMP, payload, 64);
}

#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "../log/logger.h"
static uint16_t packet_id = 1;
void ipv4_init(void) {
    klog(LOG_WARNING, "IPv4 transmit helpers initialized; inbound IPv4/ICMP processing is not implemented");
}
uint16_t net_checksum(const void* data, uint32_t len) {
    if (!data) return 0;
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint32_t)bytes[0] << 8) | bytes[1];
        bytes += 2; len -= 2;
    }
    if (len) sum += (uint32_t)bytes[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return htons((uint16_t)~sum);
}
int ipv4_send_packet(netdev_t* dev, uint32_t dest_ip, uint8_t protocol,
                     const uint8_t* payload, uint32_t payload_len) {
    if (!dev || !payload || !dev->send_packet || !dest_ip ||
        payload_len > 1480u) return -1;
    if (!dev->ip_addr || !dev->netmask) {
        klog(LOG_WARNING, "IPv4 transmit blocked: interface has no verified address configuration");
        return -2;
    }
    uint8_t packet[1500];
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    ip->ihl_version = 0x45;
    ip->tos = 0;
    ip->total_length = htons((uint16_t)(sizeof(ipv4_header_t) + payload_len));
    ip->id = htons(packet_id++);
    ip->fragment_offset = htons(0);
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = dev->ip_addr;
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = net_checksum(ip, sizeof(ipv4_header_t));
    for (uint32_t i = 0; i < payload_len; ++i)
        packet[sizeof(ipv4_header_t) + i] = payload[i];

    uint32_t next_hop = ((dest_ip & dev->netmask) == (dev->ip_addr & dev->netmask))
        ? dest_ip : dev->gateway;
    if (!next_hop) return -2;
    uint8_t target_mac[ETH_ALEN];
    if (arp_lookup(next_hop, target_mac) != 0) {
        (void)arp_send_request(dev, next_hop);
        klog(LOG_WARNING, "IPv4 transmit deferred: no verified ARP entry; receive-side ARP learning is not implemented");
        return -2;
    }
    return ethernet_send_frame(dev, target_mac, ETHERTYPE_IPV4,
                               packet, (uint32_t)sizeof(ipv4_header_t) + payload_len);
}
int icmp_ping(netdev_t* dev, uint32_t dest_ip) {
    if (!dev || !dest_ip) return -1;
    uint8_t payload[64] = {0};
    icmp_header_t* icmp = (icmp_header_t*)payload;
    icmp->type = 8; icmp->code = 0; icmp->checksum = 0;
    icmp->id = htons(0x1234); icmp->sequence = htons(1);
    for (int i = (int)sizeof(icmp_header_t); i < (int)sizeof(payload); ++i)
        payload[i] = (uint8_t)('A' + (i % 26));
    icmp->checksum = net_checksum(icmp, sizeof(payload));
    return ipv4_send_packet(dev, dest_ip, IPPROTO_ICMP, payload, sizeof(payload));
}

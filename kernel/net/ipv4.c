#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "udp.h"
#include "../log/logger.h"

static uint16_t packet_id = 1;

void ipv4_init(void) {
    klog(LOG_INFO, "IPv4 receive validation, ICMP echo handling, and UDP packet dispatch initialized; TCP receive remains unavailable");
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

static int checksum_valid(const void* data, uint32_t length) {
    return net_checksum(data, length) == 0;
}

int ipv4_receive_packet(netdev_t* dev, const uint8_t* packet, uint32_t length) {
    if (!dev || !packet || length < sizeof(ipv4_header_t) || length > 1500u)
        return -1;

    const ipv4_header_t* ip = (const ipv4_header_t*)packet;
    uint8_t version = (uint8_t)(ip->ihl_version >> 4);
    uint8_t ihl_words = (uint8_t)(ip->ihl_version & 0x0Fu);
    if (version != 4 || ihl_words < 5) return -1;

    uint32_t header_length = (uint32_t)ihl_words * 4u;
    if (header_length > length) return -1;
    uint16_t total_length = ntohs(ip->total_length);
    if (total_length < header_length || total_length > length) return -1;
    if (!checksum_valid(packet, header_length)) return -1;

    /* Reject fragmented packets until reassembly is implemented. */
    uint16_t fragment = ntohs(ip->fragment_offset);
    if ((fragment & 0x3FFFu) != 0) return -2;

    int limited_broadcast_udp = (ip->protocol == IPPROTO_UDP && ip->dest_ip == UINT32_MAX);
    if ((!dev->ip_addr || ip->dest_ip != dev->ip_addr) && !limited_broadcast_udp) return -2;

    uint32_t payload_length = (uint32_t)total_length - header_length;
    if (ip->protocol == IPPROTO_UDP)
        return udp_receive_packet(dev, ip->src_ip, ip->dest_ip,
                                  packet + header_length, payload_length);
    if (ip->protocol != IPPROTO_ICMP) return -3;
    if (payload_length < sizeof(icmp_header_t)) return -1;
    const uint8_t* icmp_bytes = packet + header_length;
    if (!checksum_valid(icmp_bytes, payload_length)) return -1;

    const icmp_header_t* request = (const icmp_header_t*)icmp_bytes;
    if (request->type != 8 || request->code != 0) return -3;
    if (!ip->src_ip || ip->src_ip == UINT32_MAX) return -1;

    uint8_t reply[1480];
    if (payload_length > sizeof(reply)) return -1;
    for (uint32_t i = 0; i < payload_length; ++i) reply[i] = icmp_bytes[i];
    icmp_header_t* response = (icmp_header_t*)reply;
    response->type = 0;
    response->checksum = 0;
    response->checksum = net_checksum(reply, payload_length);

    return ipv4_send_packet(dev, ip->src_ip, IPPROTO_ICMP, reply, payload_length);
}

int ipv4_send_packet(netdev_t* dev, uint32_t dest_ip, uint8_t protocol,
                     const uint8_t* payload, uint32_t payload_len) {
    if (!dev || !payload || !dev->send_packet || !dest_ip ||
        payload_len > 1480u) return -1;
    int bootstrap_broadcast = (!dev->ip_addr && dest_ip == UINT32_MAX &&
                               protocol == IPPROTO_UDP);
    if ((!dev->ip_addr || !dev->netmask) && !bootstrap_broadcast) {
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
    ip->dest_ip = dest_ip;
    ip->checksum = net_checksum(ip, sizeof(ipv4_header_t));
    for (uint32_t i = 0; i < payload_len; ++i)
        packet[sizeof(ipv4_header_t) + i] = payload[i];

    uint8_t target_mac[ETH_ALEN];
    if (dest_ip == UINT32_MAX) {
        for (uint32_t i = 0; i < ETH_ALEN; ++i) target_mac[i] = 0xFFu;
    } else {
        uint32_t next_hop = ((dest_ip & dev->netmask) == (dev->ip_addr & dev->netmask))
            ? dest_ip : dev->gateway;
        if (!next_hop) return -2;
        if (arp_lookup(next_hop, target_mac) != 0) {
            (void)arp_send_request(dev, next_hop);
            klog(LOG_WARNING, "IPv4 transmit deferred: no verified ARP entry");
            return -2;
        }
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

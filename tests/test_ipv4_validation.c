#include "../kernel/net/ipv4.h"
#include "../kernel/net/ethernet.h"
#include "../kernel/net/arp.h"
#include "../kernel/net/udp.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

static uint8_t sent_packet[1500];
static uint32_t sent_length;
static unsigned sent_count;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint16_t htons(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }
uint16_t ntohs(uint16_t value) { return htons(value); }
int arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]) {
    (void)ip;
    static const uint8_t mac[ETH_ALEN] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
    if (!mac_out) return -1;
    memcpy(mac_out, mac, ETH_ALEN);
    return 0;
}
int arp_send_request(netdev_t* dev, uint32_t target_ip) { (void)dev; (void)target_ip; return 1; }
int ethernet_send_frame(netdev_t* dev, const uint8_t dest_mac[ETH_ALEN],
                        uint16_t ethertype, const uint8_t* payload, uint32_t payload_len) {
    (void)dev; (void)dest_mac;
    if (ethertype != ETHERTYPE_IPV4 || !payload || payload_len > sizeof(sent_packet)) return -1;
    memcpy(sent_packet, payload, payload_len);
    sent_length = payload_len;
    ++sent_count;
    return (int)payload_len;
}
static int fake_send(netdev_t* dev, const uint8_t* packet, uint32_t length) {
    (void)dev; (void)packet;
    return (int)length;
}
static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
static void make_packet(uint8_t* packet, uint32_t* length, uint32_t src, uint32_t dst) {
    memset(packet, 0, 128);
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    ip->ihl_version = 0x45;
    ip->total_length = htons((uint16_t)(sizeof(*ip) + 16u));
    ip->id = htons(7);
    ip->fragment_offset = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_ICMP;
    ip->src_ip = src;
    ip->dest_ip = dst;
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(*ip));
    uint8_t* body = packet + sizeof(*ip);
    icmp_header_t* icmp = (icmp_header_t*)body;
    icmp->type = 8;
    icmp->code = 0;
    icmp->id = htons(0x1234);
    icmp->sequence = htons(9);
    for (unsigned i = sizeof(*icmp); i < 16u; ++i) body[i] = (uint8_t)i;
    icmp->checksum = 0;
    icmp->checksum = net_checksum(body, 16u);
    *length = sizeof(*ip) + 16u;
}
static unsigned tcp_delivery_count;
int tcp_receive_packet(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                       const uint8_t* payload, uint32_t payload_length) {
    (void)dev; (void)source_ip; (void)destination_ip;
    if (!payload || payload_length < 20u) return -1;
    ++tcp_delivery_count;
    return 77;
}

static unsigned udp_delivery_count;
static uint32_t udp_delivered_length;
static int test_udp_delivery(netdev_t* dev, uint32_t source_ip, uint16_t source_port,
                             uint16_t destination_port, const uint8_t* payload,
                             uint32_t payload_length, void* context) {
    (void)dev; (void)source_ip; (void)context;
    if (source_port != 53001 || destination_port != 61000 ||
        payload_length != 3 || memcmp(payload, "dns", 3) != 0) return -1;
    ++udp_delivery_count;
    udp_delivered_length = payload_length;
    return (int)payload_length;
}

#define CHECK(label, expr) do { if (!(expr)) { fprintf(stderr, "FAIL: %s\\n", label); return 1; } printf("PASS: %s\\n", label); } while (0)

int main(void) {
    netdev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.ip_addr = ip4(192, 168, 1, 10);
    dev.netmask = ip4(255, 255, 255, 0);
    dev.send_packet = fake_send;
    uint8_t packet[128];
    uint32_t length = 0;
    make_packet(packet, &length, ip4(192, 168, 1, 20), dev.ip_addr);
    sent_count = 0;
    CHECK("valid ICMP echo request is answered", ipv4_receive_packet(&dev, packet, length) > 0);
    CHECK("reply was emitted once", sent_count == 1);
    CHECK("reply swaps source and destination", ((ipv4_header_t*)sent_packet)->src_ip == dev.ip_addr &&
          ((ipv4_header_t*)sent_packet)->dest_ip == ip4(192, 168, 1, 20));
    CHECK("reply IPv4 checksum validates", net_checksum(sent_packet, sizeof(ipv4_header_t)) == 0);
    CHECK("reply is ICMP echo reply", ((icmp_header_t*)(sent_packet + sizeof(ipv4_header_t)))->type == 0);
    CHECK("reply ICMP checksum validates", net_checksum(sent_packet + sizeof(ipv4_header_t), 16u) == 0);

    sent_count = 0;
    CHECK("truncated packet is rejected", ipv4_receive_packet(&dev, packet, sizeof(ipv4_header_t) - 1u) == -1);
    CHECK("bad IPv4 checksum is rejected", (packet[8] ^= 1, ipv4_receive_packet(&dev, packet, length) == -1));
    packet[8] ^= 1;

    make_packet(packet, &length, ip4(192, 168, 1, 20), dev.ip_addr);
    ((ipv4_header_t*)packet)->total_length = htons(200u);
    CHECK("declared length beyond received bytes is rejected", ipv4_receive_packet(&dev, packet, length) == -1);

    make_packet(packet, &length, ip4(192, 168, 1, 20), dev.ip_addr);
    ((ipv4_header_t*)packet)->fragment_offset = htons(0x2000u);
    ((ipv4_header_t*)packet)->checksum = 0;
    ((ipv4_header_t*)packet)->checksum = net_checksum(packet, sizeof(ipv4_header_t));
    CHECK("fragmented IPv4 packet is rejected until reassembly exists",
          ipv4_receive_packet(&dev, packet, length) == -2);

    make_packet(packet, &length, ip4(192, 168, 1, 20), ip4(192, 168, 1, 99));
    CHECK("packet addressed to another host is not answered",
          ipv4_receive_packet(&dev, packet, length) == -2 && sent_count == 0);
    udp_init();
    CHECK("IPv4 test registers UDP receive port", udp_register_handler(61000, test_udp_delivery, 0) == 0);
    uint8_t udp_ip_packet[64] = {0};
    ipv4_header_t* udp_ip = (ipv4_header_t*)udp_ip_packet;
    udp_header_t* udp = (udp_header_t*)(udp_ip_packet + sizeof(ipv4_header_t));
    udp_ip->ihl_version = 0x45;
    udp_ip->total_length = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(udp_header_t) + 3u));
    udp_ip->id = htons(12);
    udp_ip->ttl = 64;
    udp_ip->protocol = IPPROTO_UDP;
    udp_ip->src_ip = ip4(192, 168, 1, 20);
    udp_ip->dest_ip = dev.ip_addr;
    udp_ip->checksum = 0;
    udp_ip->checksum = net_checksum(udp_ip, sizeof(*udp_ip));
    udp->src_port = htons(53001);
    udp->dest_port = htons(61000);
    udp->length = htons((uint16_t)(sizeof(udp_header_t) + 3u));
    udp->checksum = 0;
    memcpy(udp_ip_packet + sizeof(*udp_ip) + sizeof(*udp), "dns", 3);
    CHECK("validated IPv4 dispatches UDP payload to the registered handler",
          ipv4_receive_packet(&dev, udp_ip_packet,
            sizeof(ipv4_header_t) + sizeof(udp_header_t) + 3u) == 3);
    CHECK("UDP handler receives one payload of the expected size",
          udp_delivery_count == 1 && udp_delivered_length == 3);

    memset(udp_ip_packet, 0, sizeof(udp_ip_packet));
    ipv4_header_t* tcp_ip = (ipv4_header_t*)udp_ip_packet;
    tcp_ip->ihl_version = 0x45;
    tcp_ip->total_length = htons((uint16_t)(sizeof(ipv4_header_t) + 20u));
    tcp_ip->id = htons(13u);
    tcp_ip->ttl = 64;
    tcp_ip->protocol = IPPROTO_TCP;
    tcp_ip->src_ip = ip4(192, 168, 1, 20);
    tcp_ip->dest_ip = dev.ip_addr;
    tcp_ip->checksum = 0;
    tcp_ip->checksum = net_checksum(tcp_ip, sizeof(*tcp_ip));
    ((uint8_t*)tcp_ip)[sizeof(*tcp_ip) + 0] = 0xD4;
    ((uint8_t*)tcp_ip)[sizeof(*tcp_ip) + 1] = 0x31;
    ((uint8_t*)tcp_ip)[sizeof(*tcp_ip) + 2] = 0x1F;
    ((uint8_t*)tcp_ip)[sizeof(*tcp_ip) + 3] = 0x90;
    CHECK("IPv4 forwards TCP payload to the TCP receive handler",
          ipv4_receive_packet(&dev, udp_ip_packet, sizeof(ipv4_header_t) + 20u) == 77 &&
          tcp_delivery_count == 1);

    uint32_t configured_ip = dev.ip_addr;
    uint32_t configured_mask = dev.netmask;
    dev.ip_addr = 0;
    dev.netmask = 0;
    uint8_t bootstrap_udp[sizeof(udp_header_t) + 1u] = {0};
    udp_header_t* boot_udp = (udp_header_t*)bootstrap_udp;
    boot_udp->src_port = htons(53001u);
    boot_udp->dest_port = htons(61000u);
    boot_udp->length = htons((uint16_t)sizeof(bootstrap_udp));
    sent_count = 0;
    CHECK("unconfigured interface cannot transmit arbitrary UDP broadcasts",
          ipv4_send_packet(&dev, UINT32_MAX, IPPROTO_UDP, bootstrap_udp,
                           sizeof(bootstrap_udp)) == -2 && sent_count == 0);
    boot_udp->src_port = htons(68u);
    boot_udp->dest_port = htons(67u);
    CHECK("unconfigured interface may send DHCP client broadcast",
          ipv4_send_packet(&dev, UINT32_MAX, IPPROTO_UDP, bootstrap_udp,
                           sizeof(bootstrap_udp)) > 0 && sent_count == 1);

    uint8_t broadcast_packet[64] = {0};
    ipv4_header_t* broadcast_ip = (ipv4_header_t*)broadcast_packet;
    udp_header_t* broadcast_udp = (udp_header_t*)(broadcast_packet + sizeof(*broadcast_ip));
    broadcast_ip->ihl_version = 0x45;
    broadcast_ip->total_length = htons((uint16_t)(sizeof(*broadcast_ip) + sizeof(*broadcast_udp)));
    broadcast_ip->ttl = 64;
    broadcast_ip->protocol = IPPROTO_UDP;
    broadcast_ip->src_ip = ip4(192, 168, 1, 20);
    broadcast_ip->dest_ip = UINT32_MAX;
    broadcast_ip->checksum = 0;
    broadcast_ip->checksum = net_checksum(broadcast_ip, sizeof(*broadcast_ip));
    broadcast_udp->src_port = htons(53001u);
    broadcast_udp->dest_port = htons(61000u);
    broadcast_udp->length = htons((uint16_t)sizeof(*broadcast_udp));
    broadcast_udp->checksum = 0;
    CHECK("unconfigured interface rejects non-DHCP UDP broadcast replies",
          ipv4_receive_packet(&dev, broadcast_packet,
            sizeof(*broadcast_ip) + sizeof(*broadcast_udp)) == -2);
    dev.ip_addr = configured_ip;
    dev.netmask = configured_mask;

    printf("IPv4 receive validation tests passed.\n");
    return 0;
}

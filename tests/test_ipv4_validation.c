#include "../kernel/net/ipv4.h"
#include "../kernel/net/ethernet.h"
#include "../kernel/net/arp.h"
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
#define CHECK(label, expr) do { if (!(expr)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } printf("PASS: %s\n", label); } while (0)

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
          ipv4_receive_packet(&dev, packet, length) == -2 && sent_count == 1);
    printf("IPv4 receive validation tests passed.\n");
    return 0;
}

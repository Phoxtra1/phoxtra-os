#include "../kernel/net/arp.h"
#include "../kernel/net/ethernet.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

static uint8_t last_tx[MAX_ETH_FRAME_LEN];
static uint32_t last_tx_len;
static unsigned tx_count;
static uint64_t fake_ticks;
uint64_t timer_get_ticks(void) { return fake_ticks; }

void klog(log_level_t level, const char* message) {
    (void)level;
    (void)message;
}
uint16_t htons(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }
uint16_t ntohs(uint16_t value) { return htons(value); }

static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
static int fake_send(netdev_t* dev, const uint8_t* buffer, uint32_t length) {
    (void)dev;
    if (!buffer || length > sizeof(last_tx)) return -1;
    memcpy(last_tx, buffer, length);
    last_tx_len = length;
    ++tx_count;
    return (int)length;
}
static void setup_device(netdev_t* dev) {
    static const uint8_t mac[ETH_ALEN] = {0x02, 0x10, 0x20, 0x30, 0x40, 0x50};
    memset(dev, 0, sizeof(*dev));
    memcpy(dev->mac_addr, mac, sizeof(mac));
    dev->ip_addr = ip4(192, 168, 1, 10);
    dev->netmask = ip4(255, 255, 255, 0);
    dev->send_packet = fake_send;
    last_tx_len = 0;
    tx_count = 0;
}
static uint32_t make_arp_frame(uint8_t* frame, const uint8_t eth_src[ETH_ALEN],
                               const uint8_t arp_src[ETH_ALEN], uint16_t op,
                               uint32_t sender_ip, uint32_t target_ip,
                               const uint8_t target_mac[ETH_ALEN]) {
    eth_header_t* eth = (eth_header_t*)frame;
    arp_header_t* arp = (arp_header_t*)(frame + sizeof(*eth));
    static const uint8_t broadcast[ETH_ALEN] = {255,255,255,255,255,255};
    memset(frame, 0, MAX_ETH_FRAME_LEN);
    memcpy(eth->dest_mac, broadcast, ETH_ALEN);
    memcpy(eth->src_mac, eth_src, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_ARP);
    arp->hw_type = htons(1);
    arp->proto_type = htons(ETHERTYPE_IPV4);
    arp->hw_size = ETH_ALEN;
    arp->proto_size = 4;
    arp->opcode = htons(op);
    memcpy(arp->sender_mac, arp_src, ETH_ALEN);
    arp->sender_ip = sender_ip;
    arp->target_ip = target_ip;
    if (target_mac) memcpy(arp->target_mac, target_mac, ETH_ALEN);
    return (uint32_t)(sizeof(*eth) + sizeof(*arp));
}
#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

int main(void) {
    netdev_t dev;
    uint8_t frame[MAX_ETH_FRAME_LEN];
    uint8_t found[ETH_ALEN];
    const uint8_t peer_mac[ETH_ALEN] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
    const uint8_t multicast_mac[ETH_ALEN] = {0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};
    const uint8_t broadcast_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const uint32_t peer_ip = ip4(192, 168, 1, 20);
    const uint32_t off_link_ip = ip4(10, 0, 0, 8);
    const uint32_t our_ip = ip4(192, 168, 1, 10);

    setup_device(&dev);
    arp_init();
    uint32_t len = make_arp_frame(frame, peer_mac, peer_mac, ARP_OP_REQUEST,
                                  peer_ip, our_ip, NULL);
    CHECK("valid on-link ARP request receives a reply",
          ethernet_receive_frame(&dev, frame, len) == (int)(sizeof(eth_header_t) + sizeof(arp_header_t)));
    CHECK("ARP reply uses the peer MAC as destination",
          last_tx_len == sizeof(eth_header_t) + sizeof(arp_header_t) &&
          memcmp(((eth_header_t*)last_tx)->dest_mac, peer_mac, ETH_ALEN) == 0);
    CHECK("ARP reply opcode is correct",
          ntohs(((arp_header_t*)(last_tx + sizeof(eth_header_t)))->opcode) == ARP_OP_REPLY);
    CHECK("valid on-link sender is learned",
          arp_lookup(peer_ip, found) == 0 && memcmp(found, peer_mac, ETH_ALEN) == 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, multicast_mac, multicast_mac, ARP_OP_REQUEST, peer_ip, our_ip, NULL);
    CHECK("multicast sender MAC is rejected", ethernet_receive_frame(&dev, frame, len) == -1);
    CHECK("invalid multicast MAC is not learned", arp_lookup(peer_ip, found) != 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, peer_mac, peer_mac, ARP_OP_REPLY,
                         peer_ip, ip4(192, 168, 1, 99), dev.mac_addr);
    CHECK("unsolicited ARP reply addressed to another IP is rejected",
          ethernet_receive_frame(&dev, frame, len) == -1);
    CHECK("unsolicited ARP reply cannot poison cache", arp_lookup(peer_ip, found) != 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, peer_mac, peer_mac, ARP_OP_REQUEST, off_link_ip, our_ip, NULL);
    CHECK("off-link ARP request is not treated as a local peer",
          ethernet_receive_frame(&dev, frame, len) == 0 && tx_count == 0);
    CHECK("off-link sender is not learned", arp_lookup(off_link_ip, found) != 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, peer_mac, peer_mac, ARP_OP_REPLY, peer_ip, our_ip, dev.mac_addr);
    CHECK("ARP reply addressed to this interface is accepted",
          ethernet_receive_frame(&dev, frame, len) == 0);
    CHECK("valid ARP reply learns the peer",
          arp_lookup(peer_ip, found) == 0 && memcmp(found, peer_mac, ETH_ALEN) == 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, peer_mac, multicast_mac, ARP_OP_REQUEST, peer_ip, our_ip, NULL);
    CHECK("Ethernet/ARP sender identity mismatch is rejected",
          ethernet_receive_frame(&dev, frame, len) == -1);
    CHECK("mismatched sender is not learned", arp_lookup(peer_ip, found) != 0);

    arp_init(); setup_device(&dev);
    len = make_arp_frame(frame, broadcast_mac, broadcast_mac, ARP_OP_REQUEST, peer_ip, our_ip, NULL);
    CHECK("broadcast sender MAC is rejected", ethernet_receive_frame(&dev, frame, len) == -1);
    CHECK("broadcast sender is not learned", arp_lookup(peer_ip, found) != 0);

    printf("ARP validation regression tests passed.\n");
    return 0;
}

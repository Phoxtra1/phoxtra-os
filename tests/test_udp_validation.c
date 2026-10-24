#include "../kernel/net/udp.h"
#include "../kernel/net/ipv4.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

typedef struct receive_record {
    unsigned count;
    uint32_t source_ip;
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t payload_length;
    uint8_t payload[64];
} receive_record_t;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint16_t htons(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }
uint16_t ntohs(uint16_t value) { return htons(value); }
uint16_t net_checksum(const void* data, uint32_t length) {
    if (!data) return 0;
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t sum = 0;
    while (length > 1) {
        sum += ((uint32_t)bytes[0] << 8) | bytes[1];
        bytes += 2; length -= 2;
    }
    if (length) sum += (uint32_t)bytes[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return htons((uint16_t)~sum);
}
int ipv4_send_packet(netdev_t* dev, uint32_t destination_ip, uint8_t protocol,
                     const uint8_t* payload, uint32_t payload_length) {
    (void)dev; (void)destination_ip; (void)protocol; (void)payload;
    return (int)payload_length;
}
static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
static int on_datagram(netdev_t* dev, uint32_t source_ip, uint16_t source_port,
                       uint16_t destination_port, const uint8_t* payload,
                       uint32_t payload_length, void* context) {
    (void)dev;
    receive_record_t* record = (receive_record_t*)context;
    if (!record || payload_length > sizeof(record->payload)) return -1;
    ++record->count;
    record->source_ip = source_ip;
    record->source_port = source_port;
    record->destination_port = destination_port;
    record->payload_length = payload_length;
    memcpy(record->payload, payload, payload_length);
    return (int)payload_length;
}
static uint32_t build_packet(uint8_t* packet, uint32_t source_ip, uint32_t destination_ip,
                             uint16_t source_port, uint16_t destination_port,
                             const uint8_t* payload, uint32_t payload_length,
                             int include_checksum) {
    udp_header_t* header = (udp_header_t*)packet;
    uint32_t length = (uint32_t)sizeof(*header) + payload_length;
    header->src_port = htons(source_port);
    header->dest_port = htons(destination_port);
    header->length = htons((uint16_t)length);
    header->checksum = 0;
    if (payload_length) memcpy(packet + sizeof(*header), payload, payload_length);
    if (include_checksum) {
        uint8_t pseudo[12u + sizeof(udp_header_t) + 64u];
        memcpy(pseudo, &source_ip, 4);
        memcpy(pseudo + 4, &destination_ip, 4);
        pseudo[8] = 0;
        pseudo[9] = IPPROTO_UDP;
        memcpy(pseudo + 10, &header->length, 2);
        memcpy(pseudo + 12, packet, length);
        header->checksum = net_checksum(pseudo, 12u + length);
        if (header->checksum == 0) header->checksum = 0xFFFFu;
    }
    return length;
}
#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

int main(void) {
    netdev_t dev;
    memset(&dev, 0, sizeof(dev));
    receive_record_t record;
    memset(&record, 0, sizeof(record));
    const uint32_t source_ip = ip4(192, 168, 1, 20);
    const uint32_t destination_ip = ip4(192, 168, 1, 10);
    const uint8_t body[] = {'h','e','l','l','o'};
    uint8_t packet[128] = {0};

    udp_init();
    CHECK("UDP rejects zero destination port", udp_register_handler(0, on_datagram, &record) == -1);
    CHECK("UDP registers a bound handler", udp_register_handler(53000, on_datagram, &record) == 0);
    CHECK("UDP rejects duplicate port registration", udp_register_handler(53000, on_datagram, &record) == -2);
    uint32_t length = build_packet(packet, source_ip, destination_ip, 53001, 53000,
                                   body, sizeof(body), 1);
    CHECK("valid checksummed datagram reaches its handler",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, length) == (int)sizeof(body));
    CHECK("handler receives correct addressing and payload",
          record.count == 1 && record.source_ip == source_ip && record.source_port == 53001 &&
          record.destination_port == 53000 && record.payload_length == sizeof(body) &&
          memcmp(record.payload, body, sizeof(body)) == 0);

    CHECK("truncated UDP header is rejected",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, sizeof(udp_header_t)-1u) == -1);
    ((udp_header_t*)packet)->length = htons((uint16_t)(length + 20u));
    CHECK("UDP length beyond the received IP payload is rejected",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, length) == -1);

    length = build_packet(packet, source_ip, destination_ip, 53001, 53000,
                          body, sizeof(body), 1);
    packet[sizeof(udp_header_t)] ^= 0x01u;
    CHECK("invalid UDP checksum is rejected", udp_receive_packet(&dev, source_ip,
          destination_ip, packet, length) == -1);
    CHECK("invalid packet is not delivered", record.count == 1);

    length = build_packet(packet, source_ip, destination_ip, 53001, 53000,
                          body, sizeof(body), 0);
    CHECK("IPv4 UDP datagram with omitted checksum is accepted",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, length) == (int)sizeof(body));
    CHECK("unbound UDP port is safely ignored",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, length) == 0);

    CHECK("registered handler can be removed",
          udp_unregister_handler(53000, on_datagram, &record) == 0);
    CHECK("removed handler no longer receives datagrams",
          udp_receive_packet(&dev, source_ip, destination_ip, packet, length) == 0 && record.count == 2);
    printf("UDP receive validation tests passed.\n");
    return 0;
}

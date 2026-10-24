#include "udp.h"
#include "ipv4.h"
#include "../log/logger.h"

#define UDP_MAX_HANDLERS 16u
#define UDP_MAX_PAYLOAD 1472u

typedef struct udp_handler_entry {
    uint16_t port;
    udp_receive_handler_t handler;
    void* context;
} udp_handler_entry_t;

static udp_handler_entry_t handlers[UDP_MAX_HANDLERS];

static int udp_checksum_valid(uint32_t source_ip, uint32_t destination_ip,
                              const uint8_t* packet, uint32_t length) {
    /* A zero UDP checksum means checksum omitted for IPv4, as permitted by RFC 768. */
    const udp_header_t* header = (const udp_header_t*)packet;
    if (header->checksum == 0) return 1;
    if (length > UDP_MAX_PAYLOAD + sizeof(udp_header_t)) return 0;

    uint8_t pseudo[12u + UDP_MAX_PAYLOAD + sizeof(udp_header_t)];
    for (uint32_t i = 0; i < 4u; ++i) {
        pseudo[i] = ((const uint8_t*)&source_ip)[i];
        pseudo[4u + i] = ((const uint8_t*)&destination_ip)[i];
    }
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_UDP;
    pseudo[10] = ((const uint8_t*)&header->length)[0];
    pseudo[11] = ((const uint8_t*)&header->length)[1];
    for (uint32_t i = 0; i < length; ++i) pseudo[12u + i] = packet[i];
    return net_checksum(pseudo, 12u + length) == 0;
}

void udp_init(void) {
    for (uint32_t i = 0; i < UDP_MAX_HANDLERS; ++i) {
        handlers[i].port = 0;
        handlers[i].handler = 0;
        handlers[i].context = 0;
    }
    klog(LOG_INFO, "UDP initialized with bounded receive validation and per-port handlers");
}

int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (!port || !handler) return -1;
    for (uint32_t i = 0; i < UDP_MAX_HANDLERS; ++i) {
        if (handlers[i].port == port) return -2;
    }
    for (uint32_t i = 0; i < UDP_MAX_HANDLERS; ++i) {
        if (!handlers[i].handler) {
            handlers[i].port = port;
            handlers[i].handler = handler;
            handlers[i].context = context;
            return 0;
        }
    }
    return -3;
}

int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (!port || !handler) return -1;
    for (uint32_t i = 0; i < UDP_MAX_HANDLERS; ++i) {
        if (handlers[i].port == port && handlers[i].handler == handler &&
            handlers[i].context == context) {
            handlers[i].port = 0;
            handlers[i].handler = 0;
            handlers[i].context = 0;
            return 0;
        }
    }
    return -2;
}

int udp_receive_packet(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                       const uint8_t* packet, uint32_t length) {
    (void)destination_ip;
    if (!dev || !packet || length < sizeof(udp_header_t) ||
        length > UDP_MAX_PAYLOAD + sizeof(udp_header_t)) return -1;
    const udp_header_t* header = (const udp_header_t*)packet;
    uint16_t source_port = ntohs(header->src_port);
    uint16_t destination_port = ntohs(header->dest_port);
    uint16_t declared_length = ntohs(header->length);
    if (!source_port || !destination_port ||
        declared_length < sizeof(udp_header_t) || declared_length > length ||
        declared_length > UDP_MAX_PAYLOAD + sizeof(udp_header_t)) return -1;
    if (!udp_checksum_valid(source_ip, destination_ip, packet, declared_length)) return -1;

    for (uint32_t i = 0; i < UDP_MAX_HANDLERS; ++i) {
        if (handlers[i].handler && handlers[i].port == destination_port) {
            return handlers[i].handler(dev, source_ip, source_port, destination_port,
                                        packet + sizeof(udp_header_t),
                                        declared_length - (uint32_t)sizeof(udp_header_t),
                                        handlers[i].context);
        }
    }
    /* A valid packet to an unbound UDP port is safely ignored. */
    return 0;
}

int udp_send_packet(netdev_t* dev, uint32_t dest_ip, uint16_t src_port,
                    uint16_t dest_port, const uint8_t* payload, uint32_t payload_len) {
    if (!dev || (!payload && payload_len != 0) || !dest_ip || !src_port || !dest_port ||
        payload_len > UDP_MAX_PAYLOAD) return -1;
    uint8_t packet[sizeof(udp_header_t) + UDP_MAX_PAYLOAD];
    udp_header_t* udp = (udp_header_t*)packet;
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons((uint16_t)(sizeof(udp_header_t) + payload_len));
    udp->checksum = 0;
    for (uint32_t i = 0; i < payload_len; ++i)
        packet[sizeof(udp_header_t) + i] = payload[i];
    return ipv4_send_packet(dev, dest_ip, IPPROTO_UDP, packet,
                            (uint32_t)sizeof(udp_header_t) + payload_len);
}

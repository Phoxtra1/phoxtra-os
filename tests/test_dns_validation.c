#include "../kernel/net/dns.h"
#include "../kernel/net/udp.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

static udp_receive_handler_t registered_handler;
static void* registered_context;
static uint16_t registered_port;
static netdev_t* active_dev;
static uint8_t last_query[512];
static uint32_t last_query_length;
static uint32_t last_destination_ip;
static uint16_t last_source_port, last_destination_port;
static uint64_t fake_ticks;
static unsigned poll_count;
static unsigned send_count;
static int response_mode;
static int invalid_id_once;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint64_t timer_get_ticks(void) { return fake_ticks++; }
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (!port || !handler || registered_handler) return -1;
    registered_port = port; registered_handler = handler; registered_context = context;
    return 0;
}
int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (port != registered_port || handler != registered_handler || context != registered_context) return -1;
    registered_port = 0; registered_handler = 0; registered_context = 0;
    return 0;
}
int udp_send_packet(netdev_t* dev, uint32_t destination_ip, uint16_t source_port,
                    uint16_t destination_port, const uint8_t* payload, uint32_t length) {
    if (!dev || !payload || length > sizeof(last_query)) return -1;
    active_dev = dev; last_destination_ip = destination_ip;
    last_source_port = source_port; last_destination_port = destination_port;
    last_query_length = length;
    memcpy(last_query, payload, length);
    ++send_count;
    return 0;
}
static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ip_to_uint32(a, b, c, d);
}
static int fake_send(netdev_t* dev, const uint8_t* data, uint32_t length) {
    (void)dev; (void)data; (void)length; return 0;
}
static void write16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void build_response(uint8_t* response, int invalid_id) {
    memset(response, 0, sizeof(last_query));
    response[0] = last_query[0]; response[1] = last_query[1];
    if (invalid_id) response[1] ^= 0x01u;
    response[2] = 0x81; response[3] = (uint8_t)(response_mode == 1 ? 0x83 : 0x80);
    write16(response + 4, 1u);
    write16(response + 6, response_mode == 1 ? 0u : 1u);
    memcpy(response + 12, last_query + 12, last_query_length - 12u);
    if (response_mode == 1) return;
    uint32_t o = last_query_length;
    response[o++] = 0xC0; response[o++] = 0x0C; /* Answer owner points to QNAME. */
    write16(response + o, 1u); o += 2;
    write16(response + o, 1u); o += 2;
    response[o++] = 0; response[o++] = 0; response[o++] = 0; response[o++] = 60;
    write16(response + o, 4u); o += 2;
    response[o++] = 93; response[o++] = 184; response[o++] = 216; response[o++] = 34;
}
void net_poll(void) {
    ++poll_count;
    if (!registered_handler || !active_dev || !last_query_length) return;
    uint8_t response[512];
    if (invalid_id_once) {
        build_response(response, 1);
        (void)registered_handler(active_dev, active_dev->dns_server, 53,
                                 registered_port, response,
                                 response_mode == 1 ? last_query_length : last_query_length + 16u,
                                 registered_context);
        invalid_id_once = 0;
        return;
    }
    build_response(response, 0);
    (void)registered_handler(active_dev, active_dev->dns_server, 53,
                             registered_port, response,
                             response_mode == 1 ? last_query_length : last_query_length + 16u,
                             registered_context);
}
#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

int main(void) {
    netdev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.ip_addr = ip4(192,168,1,105);
    dev.netmask = ip4(255,255,255,0);
    dev.gateway = ip4(192,168,1,1);
    dev.dns_server = ip4(192,168,1,1);
    dev.send_packet = fake_send;
    response_mode = 0; invalid_id_once = 1; fake_ticks = poll_count = send_count = 0;

    dns_init();
    CHECK("invalid hostname is rejected without sending a query",
          dns_resolve_hostname(&dev, "bad..example") == 0 && send_count == 0);
    CHECK("DNS accepts a validated A response after rejecting wrong transaction ID",
          dns_resolve_hostname(&dev, "ExAmPlE.com.") == ip4(93,184,216,34));
    CHECK("DNS query is sent to configured server on port 53",
          last_destination_ip == dev.dns_server && last_destination_port == 53 &&
          last_source_port >= 53000u && last_source_port < 54000u);
    CHECK("DNS handler is unregistered after completing a query",
          registered_handler == 0 && registered_port == 0);

    response_mode = 1; invalid_id_once = 0;
    CHECK("DNS negative response returns no address",
          dns_resolve_hostname(&dev, "no-such.example") == 0);
    CHECK("DNS negative response cleans up temporary listener",
          registered_handler == 0 && registered_port == 0);
    printf("DNS query/response validation tests passed.\n");
    return 0;
}

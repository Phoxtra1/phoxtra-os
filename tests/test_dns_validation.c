#include "../kernel/net/dns.h"
#include "../kernel/net/udp.h"
#include "../kernel/net/socket.h"
#include "../kernel/net/ipv4.h"
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
static uint8_t tcp_dns_frame[2u + 512u + 16u];
static uint32_t tcp_dns_frame_length;
static uint32_t tcp_dns_frame_offset;
static unsigned tcp_connect_count;
static unsigned tcp_close_count;
static unsigned tcp_recv_calls;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint16_t htons(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }
uint16_t ntohs(uint16_t value) { return htons(value); }
netdev_t* netdev_get_default(void) { return active_dev; }
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
int sys_socket(int domain, int type, int protocol) {
    return domain == AF_INET && type == SOCK_STREAM && protocol == IPPROTO_TCP ? 77 : -1;
}
int sys_connect(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd != 77 || !addr || addr->sin_family != AF_INET ||
        ntohs(addr->sin_port) != 53 || !active_dev ||
        addr->sin_addr != active_dev->dns_server) return -1;
    ++tcp_connect_count;
    return 0;
}
int sys_send(int sockfd, const void* data, uint32_t length, int flags) {
    (void)flags;
    if (sockfd != 77 || !data || length < 14u) return -1;
    const uint8_t* request = (const uint8_t*)data;
    uint32_t query_length = ((uint32_t)request[0] << 8) | request[1];
    if (query_length != length - 2u || query_length > sizeof(last_query)) return -1;
    memcpy(last_query, request + 2u, query_length);
    last_query_length = query_length;

    uint8_t* response = tcp_dns_frame + 2u;
    memset(response, 0, 512u);
    response[0] = last_query[0];
    response[1] = last_query[1];
    response[2] = 0x81u;
    response[3] = 0x80u;
    write16(response + 4u, 1u);
    write16(response + 6u, 1u);
    memcpy(response + 12u, last_query + 12u, query_length - 12u);
    uint32_t offset = query_length;
    response[offset++] = 0xC0u; response[offset++] = 0x0Cu;
    write16(response + offset, 1u); offset += 2u;
    write16(response + offset, 1u); offset += 2u;
    response[offset++] = 0; response[offset++] = 0; response[offset++] = 0; response[offset++] = 60;
    write16(response + offset, 4u); offset += 2u;
    response[offset++] = 1; response[offset++] = 2; response[offset++] = 3; response[offset++] = 4;
    write16(tcp_dns_frame, (uint16_t)offset);
    tcp_dns_frame_length = offset + 2u;
    tcp_dns_frame_offset = 0;
    return (int)length;
}
int sys_recv(int sockfd, void* buffer, uint32_t length, int flags) {
    (void)flags;
    if (sockfd != 77 || !buffer || !length || tcp_dns_frame_offset >= tcp_dns_frame_length)
        return -1;
    ++tcp_recv_calls;
    uint32_t remaining = tcp_dns_frame_length - tcp_dns_frame_offset;
    uint32_t n = length < remaining ? length : remaining;
    if (n > 3u) n = 3u; /* Exercise exact-read logic across fragmented stream chunks. */
    memcpy(buffer, tcp_dns_frame + tcp_dns_frame_offset, n);
    tcp_dns_frame_offset += n;
    return (int)n;
}
int sys_close(int sockfd) {
    if (sockfd != 77) return -1;
    ++tcp_close_count;
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
    response[2] = (uint8_t)(response_mode == 2 ? 0x83 : 0x81);
    response[3] = (uint8_t)(response_mode == 1 ? 0x83 : 0x80);
    write16(response + 4, 1u);
    write16(response + 6, response_mode == 0 ? 1u : 0u);
    memcpy(response + 12, last_query + 12, last_query_length - 12u);
    if (response_mode != 0) return;
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
                                 response_mode == 0 ? last_query_length + 16u : last_query_length,
                                 registered_context);
        invalid_id_once = 0;
        return;
    }
    build_response(response, 0);
    (void)registered_handler(active_dev, active_dev->dns_server, 53,
                             registered_port, response,
                             response_mode == 0 ? last_query_length + 16u : last_query_length,
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

    response_mode = 2; invalid_id_once = 0;
    tcp_connect_count = tcp_close_count = tcp_recv_calls = 0;
    CHECK("truncated UDP reply falls back to DNS over TCP",
          dns_resolve_hostname(&dev, "truncated.example") == ip4(1,2,3,4));
    CHECK("DNS TCP fallback connects to the configured server and closes socket",
          tcp_connect_count == 1 && tcp_close_count == 1);
    CHECK("DNS TCP response can be read across partial stream reads",
          tcp_recv_calls > 3u && tcp_dns_frame_offset == tcp_dns_frame_length);
    CHECK("DNS TCP fallback leaves no UDP handler registered",
          registered_handler == 0 && registered_port == 0);
    printf("DNS query/response validation tests passed.\n");
    return 0;
}

#include "../kernel/net/socket.h"
#include "../kernel/net/tcp.h"
#include "../kernel/net/ipv4.h"
#include "../kernel/net/udp.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

static netdev_t test_dev;
static uint8_t last_tx[1500];
static uint32_t last_tx_length, last_tx_dest_ip;
static unsigned tx_count;
static uint32_t peer_next_seq = 1001u;
static int handshake_replied;
static int data_ack_replied;
static int fin_replied;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
uint16_t htons(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
uint16_t ntohs(uint16_t v) { return htons(v); }
uint32_t htonl(uint32_t v) { return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24); }
uint32_t ntohl(uint32_t v) { return htonl(v); }
uint64_t timer_get_ticks(void) { static uint64_t ticks; return ++ticks; }
netdev_t* netdev_get_default(void) { return &test_dev; }
uint32_t netdev_count(void) { return 1; }
netdev_t* netdev_get_by_index(uint32_t i) { return i == 0 ? &test_dev : 0; }
int netdev_register(netdev_t* dev) { return dev ? 0 : -1; }
void netdev_init(void) {}
static int fake_send(netdev_t* dev, const uint8_t* data, uint32_t length) {
    (void)dev; (void)data; return (int)length;
}
void net_poll(void);

uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}
uint16_t net_checksum(const void* data, uint32_t length) {
    if (!data) return 0;
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (length > 1) { sum += ((uint32_t)p[0] << 8) | p[1]; p += 2; length -= 2; }
    if (length) sum += (uint32_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return htons((uint16_t)~sum);
}
int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    (void)port; (void)handler; (void)context; return 0;
}
int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    (void)port; (void)handler; (void)context; return 0;
}
int udp_send_packet(netdev_t* dev, uint32_t dest, uint16_t src_port, uint16_t dst_port,
                    const uint8_t* data, uint32_t length) {
    (void)dev; (void)dest; (void)src_port; (void)dst_port; (void)data; return (int)length;
}
int ipv4_send_packet(netdev_t* dev, uint32_t destination_ip, uint8_t protocol,
                     const uint8_t* payload, uint32_t payload_length) {
    (void)dev;
    if (protocol != IPPROTO_TCP || !payload || payload_length > sizeof(last_tx)) return -1;
    memcpy(last_tx, payload, payload_length);
    last_tx_length = payload_length;
    last_tx_dest_ip = destination_ip;
    ++tx_count;
    return 0;
}

static int test_ok(const char* label, int passed) {
    if (!passed) { fprintf(stderr, "FAIL: %s\n", label); return 0; }
    printf("PASS: %s\n", label);
    return 1;
}
static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ip_to_uint32(a, b, c, d);
}
static int build_segment(uint8_t* out, uint32_t src_ip, uint32_t dst_ip,
                         uint16_t src_port, uint16_t dst_port, uint32_t seq,
                         uint32_t ack, uint8_t flags, const uint8_t* payload,
                         uint32_t payload_length) {
    uint32_t length = (uint32_t)sizeof(tcp_header_t) + payload_length;
    if (length > 1500u) return -1;
    memset(out, 0, length);
    tcp_header_t* h = (tcp_header_t*)out;
    h->src_port = htons(src_port);
    h->dest_port = htons(dst_port);
    h->seq_num = htonl(seq);
    h->ack_num = htonl(ack);
    h->data_offset = 5u << 4;
    h->flags = flags;
    h->window_size = htons(4096u);
    h->checksum = 0;
    if (payload_length) memcpy(out + sizeof(*h), payload, payload_length);
    uint8_t pseudo[12u + 1500u];
    memcpy(pseudo, &src_ip, 4);
    memcpy(pseudo + 4, &dst_ip, 4);
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_TCP;
    pseudo[10] = (uint8_t)(length >> 8);
    pseudo[11] = (uint8_t)length;
    memcpy(pseudo + 12, out, length);
    h->checksum = net_checksum(pseudo, 12u + length);
    return (int)length;
}
static void deliver_segment(uint32_t source_ip, uint32_t destination_ip,
                            uint16_t source_port, uint16_t destination_port,
                            uint32_t seq, uint32_t ack, uint8_t flags,
                            const uint8_t* payload, uint32_t payload_length) {
    uint8_t packet[1500];
    int length = build_segment(packet, source_ip, destination_ip, source_port,
                               destination_port, seq, ack, flags, payload, payload_length);
    if (length > 0) (void)tcp_receive_packet(&test_dev, source_ip, destination_ip,
                                             packet, (uint32_t)length);
}

void net_poll(void) {
    if (last_tx_length < sizeof(tcp_header_t)) return;
    const tcp_header_t* sent = (const tcp_header_t*)last_tx;
    uint16_t source_port = ntohs(sent->src_port);
    uint16_t destination_port = ntohs(sent->dest_port);
    uint32_t sequence = ntohl(sent->seq_num);
    uint8_t flags = sent->flags;
    uint32_t payload_length = last_tx_length - sizeof(tcp_header_t);

    if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == TCP_FLAG_SYN && !handshake_replied) {
        handshake_replied = 1;
        deliver_segment(last_tx_dest_ip, test_dev.ip_addr, destination_port, source_port,
                        1000u, sequence + 1u, TCP_FLAG_SYN | TCP_FLAG_ACK, 0, 0);
        peer_next_seq = 1001u;
        return;
    }
    if ((flags & (TCP_FLAG_ACK | TCP_FLAG_PSH)) == (TCP_FLAG_ACK | TCP_FLAG_PSH) &&
        payload_length && !data_ack_replied) {
        data_ack_replied = 1;
        deliver_segment(last_tx_dest_ip, test_dev.ip_addr, destination_port, source_port,
                        peer_next_seq, sequence + payload_length, TCP_FLAG_ACK, 0, 0);
        return;
    }
    if ((flags & (TCP_FLAG_FIN | TCP_FLAG_ACK)) == (TCP_FLAG_FIN | TCP_FLAG_ACK) && !fin_replied) {
        fin_replied = 1;
        deliver_segment(last_tx_dest_ip, test_dev.ip_addr, destination_port, source_port,
                        peer_next_seq, sequence + 1u, TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
    }
}

int main(void) {
    memset(&test_dev, 0, sizeof(test_dev));
    test_dev.ip_addr = ip4(10, 20, 30, 2);
    test_dev.netmask = ip4(255, 255, 255, 0);
    test_dev.gateway = ip4(10, 20, 30, 1);
    test_dev.send_packet = fake_send;
    const uint32_t server_ip = ip4(10, 20, 30, 10);
    const uint16_t server_port = 8080u;

    socket_subsystem_init();
    int fd = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!test_ok("TCP socket allocates", fd >= 0)) return 1;
    sockaddr_in_t remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(server_port);
    remote.sin_addr = server_ip;

    if (!test_ok("TCP connect completes a SYN/SYN-ACK/ACK exchange", sys_connect(fd, &remote) == 0)) return 1;
    socket_entry_t* socket = socket_get_by_id(fd);
    if (!test_ok("connection enters ESTABLISHED state", socket && socket->state == TCP_STATE_ESTABLISHED)) return 1;
    if (!test_ok("handshake exchanged actual TCP packets", handshake_replied && tx_count >= 2)) return 1;

    const uint8_t send_body[] = {'p','h','o','x','t','r','a'};
    data_ack_replied = 0;
    if (!test_ok("TCP send returns only after peer ACK", sys_send(fd, send_body, sizeof(send_body), 0) == (int)sizeof(send_body))) return 1;

    const uint8_t receive_body[] = {'r','e','p','l','y'};
    deliver_segment(server_ip, test_dev.ip_addr, server_port, socket->local_port,
                    peer_next_seq, socket->snd_nxt, TCP_FLAG_ACK | TCP_FLAG_PSH,
                    receive_body, sizeof(receive_body));
    peer_next_seq += sizeof(receive_body);
    if (!test_ok("inbound TCP payload reaches the stream receive buffer", socket->tcp_rx_count == sizeof(receive_body))) return 1;
    uint8_t received[16] = {0};
    int got = sys_recv(fd, received, sizeof(received), 0);
    if (!test_ok("TCP receive drains payload bytes", got == (int)sizeof(receive_body) &&
                 memcmp(received, receive_body, sizeof(receive_body)) == 0)) return 1;

    uint8_t bad[64];
    int bad_len = build_segment(bad, server_ip, test_dev.ip_addr, server_port,
                                socket->local_port, peer_next_seq, socket->snd_nxt,
                                TCP_FLAG_ACK, 0, 0);
    if (!test_ok("bad TCP checksum packet can be constructed", bad_len > 0)) return 1;
    bad[16] ^= 0x20u;
    uint64_t before_seq = socket->rcv_nxt;
    if (!test_ok("invalid TCP checksum is rejected", tcp_receive_packet(&test_dev, server_ip,
                 test_dev.ip_addr, bad, (uint32_t)bad_len) == -1 &&
                 socket->rcv_nxt == before_seq)) return 1;

    fin_replied = 0;
    int close_result = sys_close(fd);
    if (!test_ok("TCP close performs FIN/ACK teardown when peer responds", close_result == 0 && fin_replied)) return 1;
    if (!test_ok("closed socket handle is released", socket_get_by_id(fd) == 0)) return 1;

    printf("TCP validation tests passed.\n");
    return 0;
}

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
static int defer_next_send;

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
static udp_receive_handler_t registered_udp_handler;
static void* registered_udp_context;
static uint16_t registered_udp_port;
static unsigned udp_send_count;
static uint32_t udp_send_destination;
static uint16_t udp_send_source_port, udp_send_destination_port;
int udp_register_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (!port || !handler || registered_udp_handler) return -1;
    registered_udp_port = port;
    registered_udp_handler = handler;
    registered_udp_context = context;
    return 0;
}
int udp_unregister_handler(uint16_t port, udp_receive_handler_t handler, void* context) {
    if (registered_udp_port != port || registered_udp_handler != handler ||
        registered_udp_context != context) return -1;
    registered_udp_port = 0;
    registered_udp_handler = 0;
    registered_udp_context = 0;
    return 0;
}
int udp_send_packet(netdev_t* dev, uint32_t dest, uint16_t src_port, uint16_t dst_port,
                    const uint8_t* data, uint32_t length) {
    if (!dev || (!data && length)) return -1;
    ++udp_send_count;
    udp_send_destination = dest;
    udp_send_source_port = src_port;
    udp_send_destination_port = dst_port;
    return (int)length;
}
int ipv4_send_packet(netdev_t* dev, uint32_t destination_ip, uint8_t protocol,
                     const uint8_t* payload, uint32_t payload_length) {
    (void)dev;
    if (protocol != IPPROTO_TCP || !payload || payload_length > sizeof(last_tx)) return -1;
    if (defer_next_send) { defer_next_send = 0; return -2; }
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
    defer_next_send = 1;
    if (!test_ok("TCP send retries after ARP/route deferral and waits for peer ACK",
                 sys_send(fd, send_body, sizeof(send_body), 0) == (int)sizeof(send_body) &&
                 defer_next_send == 0 && data_ack_replied)) return 1;

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

    handshake_replied = 0;
    data_ack_replied = 0;
    fin_replied = 0;
    defer_next_send = 1;
    int deferred_fd = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!test_ok("TCP connect retries a deferred initial SYN",
                 deferred_fd >= 0 && sys_connect(deferred_fd, &remote) == 0 &&
                 defer_next_send == 0 && handshake_replied)) return 1;
    if (!test_ok("deferred TCP connection reaches ESTABLISHED",
                 socket_get_by_id(deferred_fd) &&
                 socket_get_by_id(deferred_fd)->state == TCP_STATE_ESTABLISHED)) return 1;
    peer_next_seq = 1001u;
    if (!test_ok("second TCP connection closes cleanly", sys_close(deferred_fd) == 0)) return 1;

    int listener = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in_t local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(9090u);
    if (!test_ok("TCP listener socket allocates", listener >= 0)) return 1;
    if (!test_ok("TCP listener binds a local port", sys_bind(listener, &local) == 0)) return 1;
    if (!test_ok("TCP listener enters LISTEN state", sys_listen(listener, 3) == 0 &&
                 socket_get_by_id(listener)->state == TCP_STATE_LISTEN)) return 1;

    const uint16_t peer_port = 55000u;
    deliver_segment(server_ip, test_dev.ip_addr, peer_port, 9090u,
                    3000u, 0u, TCP_FLAG_SYN, 0, 0);
    int child_id = -1;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* candidate = socket_get_by_id(i);
        if (candidate && candidate->parent_id == listener &&
            candidate->state == TCP_STATE_SYN_RECEIVED) { child_id = i; break; }
    }
    if (!test_ok("passive TCP SYN creates a half-open connection", child_id >= 0)) return 1;
    socket_entry_t* child = socket_get_by_id(child_id);
    if (!test_ok("passive open transmits SYN-ACK",
                 last_tx_length >= sizeof(tcp_header_t) &&
                 (((tcp_header_t*)last_tx)->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                    (TCP_FLAG_SYN | TCP_FLAG_ACK))) return 1;

    deliver_segment(server_ip, test_dev.ip_addr, peer_port, 9090u,
                    3001u, child->snd_nxt, TCP_FLAG_ACK, 0, 0);
    if (!test_ok("valid final ACK establishes passive connection",
                 child->state == TCP_STATE_ESTABLISHED)) return 1;
    sockaddr_in_t peer;
    memset(&peer, 0, sizeof(peer));
    int accepted = sys_accept(listener, &peer);
    if (!test_ok("accept dequeues an established connection",
                 accepted == child_id && peer.sin_addr == server_ip &&
                 ntohs(peer.sin_port) == peer_port)) return 1;

    peer_next_seq = 3001u;
    fin_replied = 0;
    if (!test_ok("accepted socket closes with FIN teardown", sys_close(accepted) == 0)) return 1;
    if (!test_ok("listener closes without leaked child handles", sys_close(listener) == 0 &&
                 socket_get_by_id(listener) == 0)) return 1;

    int udp_fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in_t udp_local;
    memset(&udp_local, 0, sizeof(udp_local));
    udp_local.sin_family = AF_INET;
    udp_local.sin_port = htons(53535u);
    if (!test_ok("UDP socket allocates", udp_fd >= 0)) return 1;
    if (!test_ok("UDP socket binds and registers its receive handler",
                 sys_bind(udp_fd, &udp_local) == 0 &&
                 registered_udp_port == 53535u && registered_udp_handler != 0)) return 1;
    int duplicate_fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!test_ok("duplicate UDP local port is rejected",
                 duplicate_fd >= 0 && sys_bind(duplicate_fd, &udp_local) == -1)) return 1;
    const uint8_t first_datagram[] = {'o','n','e'};
    const uint8_t second_datagram[] = {'t','w','o'};
    if (!test_ok("UDP callback queues first datagram",
                 registered_udp_handler(&test_dev, server_ip, 55001u, 53535u,
                                        first_datagram, sizeof(first_datagram),
                                        registered_udp_context) == (int)sizeof(first_datagram))) return 1;
    if (!test_ok("UDP callback drops a second datagram while the one-slot buffer is full",
                 registered_udp_handler(&test_dev, server_ip, 55002u, 53535u,
                                        second_datagram, sizeof(second_datagram),
                                        registered_udp_context) == 0)) return 1;
    uint8_t udp_received[8] = {0};
    if (!test_ok("UDP recv returns the first queued datagram",
                 sys_recv(udp_fd, udp_received, sizeof(udp_received), 0) == (int)sizeof(first_datagram) &&
                 memcmp(udp_received, first_datagram, sizeof(first_datagram)) == 0)) return 1;
    remote.sin_port = htons(5353u);
    if (!test_ok("UDP connect records a remote endpoint", sys_connect(udp_fd, &remote) == 0)) return 1;
    udp_send_count = 0;
    const uint8_t outbound_datagram[] = {'q','u','e','r','y'};
    if (!test_ok("UDP send returns payload bytes and uses bound ports",
                 sys_send(udp_fd, outbound_datagram, sizeof(outbound_datagram), 0) ==
                    (int)sizeof(outbound_datagram) &&
                 udp_send_count == 1 && udp_send_destination == server_ip &&
                 udp_send_source_port == 53535u && udp_send_destination_port == 5353u)) return 1;
    if (!test_ok("UDP close unregisters its receive handler",
                 sys_close(udp_fd) == 0 && registered_udp_handler == 0 &&
                 registered_udp_port == 0)) return 1;
    if (!test_ok("unused socket handle can be closed", sys_close(duplicate_fd) == 0)) return 1;

    printf("TCP and socket validation tests passed.\n");
    return 0;
}

#include "socket.h"
#include "udp.h"
#include "ipv4.h"
#include "netdev.h"
#include "../include/timer.h"
#include "../log/logger.h"

#define SOCKET_WAIT_TICKS 500u
#define SOCKET_MAX_POLLS 20000000u
#define SOCKET_EPHEMERAL_FIRST 49152u
#define SOCKET_EPHEMERAL_LAST 65535u

static socket_entry_t socket_table[MAX_SOCKETS];
static uint16_t next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;

static void clear_entry(socket_entry_t* socket, int id) {
    if (!socket) return;
    uint8_t* bytes = (uint8_t*)socket;
    for (uint32_t i = 0; i < (uint32_t)sizeof(*socket); ++i) bytes[i] = 0;
    socket->id = id;
    socket->state = TCP_STATE_CLOSED;
    socket->parent_id = -1;
}

static int port_in_use(uint16_t port, int type, const socket_entry_t* exclude) {
    if (!port) return 1;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        const socket_entry_t* other = &socket_table[i];
        if (!other->in_use || other == exclude || other->type != type) continue;
        if (other->local_port == port) return 1;
    }
    return 0;
}

static int assign_ephemeral_port(socket_entry_t* socket) {
    if (!socket) return -1;
    for (uint32_t attempt = 0; attempt <=
         (uint32_t)(SOCKET_EPHEMERAL_LAST - SOCKET_EPHEMERAL_FIRST); ++attempt) {
        uint16_t port = next_ephemeral_port++;
        if (next_ephemeral_port < SOCKET_EPHEMERAL_FIRST) next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;
        if (!port_in_use(port, socket->type, socket)) {
            socket->local_port = port;
            return 0;
        }
    }
    return -1;
}

static int socket_udp_receive(netdev_t* dev, uint32_t source_ip,
                              uint16_t source_port, uint16_t destination_port,
                              const uint8_t* payload, uint32_t payload_length,
                              void* context) {
    socket_entry_t* socket = (socket_entry_t*)context;
    if (!socket || !socket->in_use || socket->type != SOCK_DGRAM ||
        !socket->udp_bound || !dev || !payload ||
        destination_port != socket->local_port ||
        payload_length > SOCKET_UDP_RECEIVE_CAPACITY) return -1;
    /* A single bounded datagram slot provides explicit back-pressure by dropping extras. */
    if (socket->udp_rx_pending) return 0;
    for (uint32_t i = 0; i < payload_length; ++i)
        socket->udp_rx_buffer[i] = payload[i];
    socket->udp_rx_length = (uint16_t)payload_length;
    socket->udp_rx_source_ip = source_ip;
    socket->udp_rx_source_port = source_port;
    socket->udp_rx_pending = 1;
    return (int)payload_length;
}

static int bind_udp_port(socket_entry_t* socket) {
    if (!socket || socket->type != SOCK_DGRAM) return -1;
    if (socket->udp_bound) return 0;
    if (!socket->local_port && assign_ephemeral_port(socket) != 0) return -1;
    int result = udp_register_handler(socket->local_port, socket_udp_receive, socket);
    if (result != 0) return -1;
    socket->udp_bound = 1;
    return 0;
}

void socket_subsystem_init(void) {
    for (int i = 0; i < MAX_SOCKETS; ++i) clear_entry(&socket_table[i], i);
    next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;
    klog(LOG_INFO, "IPv4 socket table initialized with bounded UDP datagram slots and TCP stream state");
}

int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM)) return -1;
    int required_protocol = type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
    if (protocol != 0 && protocol != required_protocol) return -1;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        if (!socket_table[i].in_use) {
            clear_entry(&socket_table[i], i);
            socket_table[i].in_use = 1;
            socket_table[i].domain = domain;
            socket_table[i].type = type;
            socket_table[i].protocol = required_protocol;
            return i;
        }
    }
    return -1;
}

int sys_bind(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || socket->state != TCP_STATE_CLOSED ||
        addr->sin_family != AF_INET) return -1;

    uint16_t port = ntohs(addr->sin_port);
    if (!port) {
        if (assign_ephemeral_port(socket) != 0) return -1;
        port = socket->local_port;
    } else {
        if (port_in_use(port, socket->type, socket)) return -1;
        socket->local_port = port;
    }

    netdev_t* dev = netdev_get_default();
    if (addr->sin_addr && (!dev || !dev->ip_addr || addr->sin_addr != dev->ip_addr)) {
        socket->local_port = 0;
        return -1;
    }
    socket->local_ip = addr->sin_addr ? addr->sin_addr : (dev ? dev->ip_addr : 0);

    if (socket->type == SOCK_DGRAM && bind_udp_port(socket) != 0) {
        socket->local_port = 0;
        return -1;
    }
    return 0;
}

int sys_connect(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || addr->sin_family != AF_INET ||
        !addr->sin_addr || !ntohs(addr->sin_port) ||
        socket->state != TCP_STATE_CLOSED) return -1;

    netdev_t* dev = netdev_get_default();
    if (!dev || !dev->ip_addr || !dev->netmask) return -2;

    socket->remote_ip = addr->sin_addr;
    socket->remote_port = ntohs(addr->sin_port);
    if (!socket->local_ip) socket->local_ip = dev->ip_addr;
    if (!socket->local_port && assign_ephemeral_port(socket) != 0) return -2;

    if (socket->type == SOCK_STREAM) return tcp_connect_socket(socket, dev);
    if (socket->type == SOCK_DGRAM) {
        if (bind_udp_port(socket) != 0) return -2;
        return 0;
    }
    return -1;
}

int sys_listen(int sockfd, int backlog) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || socket->type != SOCK_STREAM ||
        socket->state != TCP_STATE_CLOSED) return -1;
    if (!socket->local_port && assign_ephemeral_port(socket) != 0) return -2;
    if (backlog < 1) backlog = 1;
    if (backlog > (int)TCP_MAX_LISTEN_BACKLOG) backlog = (int)TCP_MAX_LISTEN_BACKLOG;
    socket->listen_backlog = (uint8_t)backlog;
    socket->pending_head = 0;
    socket->pending_count = 0;
    for (uint32_t i = 0; i < TCP_MAX_LISTEN_BACKLOG; ++i) socket->pending_children[i] = -1;
    socket->state = TCP_STATE_LISTEN;
    return 0;
}

int sys_accept(int sockfd, sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* listener = &socket_table[sockfd];
    if (!listener->in_use || listener->type != SOCK_STREAM ||
        listener->state != TCP_STATE_LISTEN) return -1;

    uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (polls++ < SOCKET_MAX_POLLS) {
        while (listener->pending_count) {
            int index = listener->pending_children[listener->pending_head];
            listener->pending_children[listener->pending_head] = -1;
            listener->pending_head = (uint8_t)((listener->pending_head + 1u) % TCP_MAX_LISTEN_BACKLOG);
            --listener->pending_count;
            socket_entry_t* child = socket_get_by_id(index);
            if (!child || child->parent_id != listener->id ||
                (child->state != TCP_STATE_ESTABLISHED && child->state != TCP_STATE_CLOSE_WAIT))
                continue;
            child->parent_id = -1;
            if (addr) {
                addr->sin_family = AF_INET;
                addr->sin_port = htons(child->remote_port);
                addr->sin_addr = child->remote_ip;
                for (uint32_t i = 0; i < sizeof(addr->sin_zero); ++i) addr->sin_zero[i] = 0;
            }
            return child->id;
        }
        net_poll();
        if (timer_get_ticks() - started >= SOCKET_WAIT_TICKS) return -2;
    }
    return -2;
}

int sys_send(int sockfd, const void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || (!buf && len)) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || !socket->remote_ip || !socket->remote_port) return -1;
    if (!len) return 0;

    netdev_t* dev = netdev_get_default();
    if (!dev) return -2;
    if (socket->type == SOCK_STREAM) {
        return tcp_send_data(socket, dev, (const uint8_t*)buf, len);
    }
    if (socket->type != SOCK_DGRAM || len > SOCKET_UDP_RECEIVE_CAPACITY) return -1;
    if (!socket->local_port && bind_udp_port(socket) != 0) return -2;
    return udp_send_packet(dev, socket->remote_ip, socket->local_port, socket->remote_port,
                           (const uint8_t*)buf, len);
}

int sys_recv(int sockfd, void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || (!buf && len)) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use) return -1;
    if (!len) return 0;

    if (socket->type == SOCK_STREAM) {
        netdev_t* dev = netdev_get_default();
        if (!dev) return -2;
        return tcp_receive_data(socket, dev, (uint8_t*)buf, len);
    }
    if (socket->type != SOCK_DGRAM || bind_udp_port(socket) != 0) return -2;

    uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (!socket->udp_rx_pending && polls++ < SOCKET_MAX_POLLS) {
        net_poll();
        if (timer_get_ticks() - started >= SOCKET_WAIT_TICKS) return -2;
    }
    if (!socket->udp_rx_pending) return -2;

    uint32_t copy_length = len < socket->udp_rx_length ? len : socket->udp_rx_length;
    for (uint32_t i = 0; i < copy_length; ++i) ((uint8_t*)buf)[i] = socket->udp_rx_buffer[i];
    socket->udp_rx_pending = 0;
    socket->udp_rx_length = 0;
    return (int)copy_length;
}

int sys_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use) return -1;

    if (socket->type == SOCK_DGRAM && socket->udp_bound) {
        (void)udp_unregister_handler(socket->local_port, socket_udp_receive, socket);
        socket->udp_bound = 0;
    }
    int result = 0;
    netdev_t* dev = netdev_get_default();
    if (socket->type == SOCK_STREAM && dev &&
        (socket->state == TCP_STATE_ESTABLISHED || socket->state == TCP_STATE_CLOSE_WAIT ||
         socket->state == TCP_STATE_FIN_WAIT || socket->state == TCP_STATE_LAST_ACK))
        result = tcp_close_socket(socket, dev);

    for (int i = 0; i < MAX_SOCKETS; ++i) {
        if (i == sockfd || !socket_table[i].in_use) continue;
        if (socket_table[i].parent_id == sockfd && socket_table[i].state != TCP_STATE_LISTEN) {
            socket_table[i].error = -1;
            socket_table[i].state = TCP_STATE_CLOSED;
            socket_table[i].in_use = 0;
        }
    }
    clear_entry(socket, sockfd);
    return result;
}

socket_entry_t* socket_get_by_id(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use) return 0;
    return &socket_table[sockfd];
}

uint32_t socket_get_active_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_SOCKETS; ++i) if (socket_table[i].in_use) ++count;
    return count;
}

#include "socket.h"
#include "udp.h"
#include "netdev.h"
#include "ipv4.h"
#include "../include/timer.h"
#include "../log/logger.h"
#include <stddef.h>

#define SOCKET_EPHEMERAL_FIRST 49152u
#define SOCKET_EPHEMERAL_LAST  65535u
#define SOCKET_IO_TIMEOUT_TICKS 500u
#define SOCKET_MAX_POLLS 20000000u

static socket_entry_t socket_table[MAX_SOCKETS];
static uint16_t next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;

static void socket_zero(void* memory, size_t length) {
    uint8_t* bytes = (uint8_t*)memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static void socket_reset(socket_entry_t* socket, int id) {
    if (!socket) return;
    socket_zero(socket, sizeof(*socket));
    socket->id = id;
    socket->state = TCP_STATE_CLOSED;
    socket->parent_id = -1;
}

static int socket_conflicts(uint16_t port, uint32_t address, int except_id) {
    if (!port) return 0;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        const socket_entry_t* other = &socket_table[i];
        if (i == except_id || !other->in_use || other->local_port != port) continue;
        if (other->local_ip == 0u || address == 0u || other->local_ip == address)
            return 1;
    }
    return 0;
}

static uint16_t allocate_ephemeral_port(int except_id, uint32_t address) {
    const uint32_t range = SOCKET_EPHEMERAL_LAST - SOCKET_EPHEMERAL_FIRST + 1u;
    for (uint32_t attempt = 0; attempt < range; ++attempt) {
        uint16_t candidate = next_ephemeral_port;
        ++next_ephemeral_port;
        if (next_ephemeral_port < SOCKET_EPHEMERAL_FIRST ||
            next_ephemeral_port > SOCKET_EPHEMERAL_LAST)
            next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;
        if (!socket_conflicts(candidate, address, except_id)) return candidate;
    }
    return 0;
}

void socket_subsystem_init(void) {
    for (int i = 0; i < MAX_SOCKETS; ++i) socket_reset(&socket_table[i], i);
    next_ephemeral_port = SOCKET_EPHEMERAL_FIRST;
    klog(LOG_INFO, "Socket subsystem ready: TCP sockets use the packet-driven transport state machine");
}

int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -1;
    if (type == SOCK_STREAM && protocol != 0 && protocol != IPPROTO_TCP) return -1;
    if (type == SOCK_DGRAM && protocol != 0 && protocol != IPPROTO_UDP) return -1;

    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* socket = &socket_table[i];
        if (socket->in_use) continue;
        socket_reset(socket, i);
        socket->in_use = 1;
        socket->domain = domain;
        socket->type = type;
        socket->protocol = (protocol != 0) ? protocol :
                           (type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
        return i;
    }
    return -1;
}

int sys_bind(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr ||
        addr->sin_family != AF_INET) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || socket->state != TCP_STATE_CLOSED) return -1;

    uint32_t local_ip = addr->sin_addr;
    netdev_t* dev = netdev_get_default();
    if (local_ip && dev && dev->ip_addr && local_ip != dev->ip_addr) return -1;

    uint16_t local_port = ntohs(addr->sin_port);
    if (!local_port) local_port = allocate_ephemeral_port(sockfd, local_ip);
    if (!local_port || socket_conflicts(local_port, local_ip, sockfd)) return -1;

    socket->local_ip = local_ip;
    socket->local_port = local_port;
    socket->udp_bound = (uint8_t)(socket->type == SOCK_DGRAM);
    return 0;
}

int sys_connect(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr ||
        addr->sin_family != AF_INET || !addr->sin_addr || !ntohs(addr->sin_port))
        return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || socket->state == TCP_STATE_LISTEN) return -1;
    netdev_t* dev = netdev_get_default();
    if (!dev || !dev->ip_addr || !(dev->flags & 0x01u)) return -1;

    if (!socket->local_port) {
        socket->local_port = allocate_ephemeral_port(sockfd, dev->ip_addr);
        if (!socket->local_port) return -1;
    }
    if (!socket->local_ip) socket->local_ip = dev->ip_addr;
    socket->remote_ip = addr->sin_addr;
    socket->remote_port = ntohs(addr->sin_port);

    if (socket->type == SOCK_DGRAM) {
        socket->state = TCP_STATE_ESTABLISHED;
        return 0;
    }
    if (socket->type != SOCK_STREAM) return -1;

    /* Success requires SYN/ACK reception and the outgoing final ACK. */
    return tcp_connect_socket(socket, dev);
}

int sys_listen(int sockfd, int backlog) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || backlog <= 0) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use || socket->type != SOCK_STREAM ||
        socket->state != TCP_STATE_CLOSED || !socket->local_port) return -1;
    if (backlog > (int)TCP_MAX_LISTEN_BACKLOG) backlog = (int)TCP_MAX_LISTEN_BACKLOG;
    socket->listen_backlog = (uint8_t)backlog;
    socket->pending_head = 0;
    socket->pending_count = 0;
    for (uint32_t i = 0; i < TCP_MAX_LISTEN_BACKLOG; ++i)
        socket->pending_children[i] = -1;
    socket->state = TCP_STATE_LISTEN;
    return 0;
}

int sys_accept(int sockfd, sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* listener = &socket_table[sockfd];
    if (!listener->in_use || listener->type != SOCK_STREAM ||
        listener->state != TCP_STATE_LISTEN) return -1;

    netdev_t* dev = netdev_get_default();
    if (!dev || !dev->receive_packet || !dev->send_packet) return -1;
    const uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (listener->in_use && listener->state == TCP_STATE_LISTEN &&
           polls++ < SOCKET_MAX_POLLS) {
        if (listener->pending_count != 0u) {
            int child_id = listener->pending_children[listener->pending_head];
            listener->pending_children[listener->pending_head] = -1;
            listener->pending_head = (uint8_t)((listener->pending_head + 1u) %
                                                 TCP_MAX_LISTEN_BACKLOG);
            --listener->pending_count;
            socket_entry_t* child = socket_get_by_id(child_id);
            if (!child || child->state != TCP_STATE_ESTABLISHED ||
                child->parent_id != sockfd) continue;
            child->parent_id = -1;
            if (addr) {
                socket_zero(addr, sizeof(*addr));
                addr->sin_family = AF_INET;
                addr->sin_port = htons(child->remote_port);
                addr->sin_addr = child->remote_ip;
            }
            return child_id;
        }
        net_poll();
        if (timer_get_ticks() - started >= SOCKET_IO_TIMEOUT_TICKS) return -2;
    }
    return -2;
}

int sys_send(int sockfd, const void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || (!buf && len != 0u)) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use) return -1;
    if (len == 0u) return 0;

    netdev_t* dev = netdev_get_default();
    if (!dev || !(dev->flags & 0x01u)) return -1;
    if (socket->type == SOCK_DGRAM) {
        if (!socket->remote_ip || !socket->remote_port) return -1;
        if (!socket->local_port) {
            socket->local_port = allocate_ephemeral_port(sockfd, socket->local_ip);
            if (!socket->local_port) return -1;
        }
        if (!socket->local_ip) socket->local_ip = dev->ip_addr;
        int result = udp_send_packet(dev, socket->remote_ip, socket->local_port,
                                     socket->remote_port, (const uint8_t*)buf, len);
        return result < 0 ? result : (int)len;
    }
    if (socket->type != SOCK_STREAM) return -1;
    return tcp_send_data(socket, dev, (const uint8_t*)buf, len);
}

int sys_recv(int sockfd, void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || (!buf && len != 0u)) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use) return -1;
    if (len == 0u) return 0;

    netdev_t* dev = netdev_get_default();
    if (!dev || !dev->receive_packet) return -1;
    if (socket->type == SOCK_STREAM)
        return tcp_receive_data(socket, dev, (uint8_t*)buf, len);
    if (socket->type != SOCK_DGRAM) return -1;

    const uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (!socket->udp_rx_pending && polls++ < SOCKET_MAX_POLLS) {
        net_poll();
        if (timer_get_ticks() - started >= SOCKET_IO_TIMEOUT_TICKS) return -2;
    }
    if (!socket->udp_rx_pending) return -2;

    uint32_t copy_length = len < socket->udp_rx_length ? len : socket->udp_rx_length;
    for (uint32_t i = 0; i < copy_length; ++i)
        ((uint8_t*)buf)[i] = socket->udp_rx_buffer[i];
    socket->udp_rx_pending = 0;
    socket->udp_rx_length = 0;
    return (int)copy_length;
}

int socket_udp_dispatch(netdev_t* dev, uint32_t source_ip, uint16_t source_port,
                        uint16_t destination_port, const uint8_t* payload,
                        uint32_t payload_length) {
    if (!dev || (!payload && payload_length != 0u) ||
        payload_length > SOCKET_UDP_RECEIVE_CAPACITY) return -1;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* socket = &socket_table[i];
        if (!socket->in_use || socket->type != SOCK_DGRAM ||
            socket->local_port != destination_port ||
            (socket->local_ip && socket->local_ip != dev->ip_addr)) continue;
        if (socket->udp_rx_pending) return 0;
        for (uint32_t j = 0; j < payload_length; ++j)
            socket->udp_rx_buffer[j] = payload[j];
        socket->udp_rx_length = (uint16_t)payload_length;
        socket->udp_rx_source_ip = source_ip;
        socket->udp_rx_source_port = source_port;
        socket->udp_rx_pending = 1;
        return 0;
    }
    return -3;
}

int sys_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* socket = &socket_table[sockfd];
    if (!socket->in_use) return -1;

    int was_listener = (socket->state == TCP_STATE_LISTEN);
    netdev_t* dev = netdev_get_default();
    int close_result = 0;
    if (socket->type == SOCK_STREAM && dev &&
        socket->state != TCP_STATE_CLOSED) {
        close_result = tcp_close_socket(socket, dev);
    }

    if (was_listener) {
        for (int i = 0; i < MAX_SOCKETS; ++i) {
            if (i == sockfd || !socket_table[i].in_use ||
                socket_table[i].parent_id != sockfd) continue;
            socket_reset(&socket_table[i], i);
        }
    }
    socket_reset(socket, sockfd);
    return close_result;
}

socket_entry_t* socket_get_by_id(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return 0;
    return socket_table[sockfd].in_use ? &socket_table[sockfd] : 0;
}

uint32_t socket_get_active_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_SOCKETS; ++i)
        if (socket_table[i].in_use) ++count;
    return count;
}

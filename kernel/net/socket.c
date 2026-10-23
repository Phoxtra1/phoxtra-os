#include "socket.h"
#include "udp.h"
#include "netdev.h"
#include "../log/logger.h"
static socket_entry_t socket_table[MAX_SOCKETS];
void socket_subsystem_init(void) {
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_table[i].in_use = 0;
        socket_table[i].id = i;
        socket_table[i].state = TCP_STATE_CLOSED;
    }
    klog(LOG_WARNING, "Socket handles initialized; TCP handshake and receive queues are not implemented");
}
int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM)) return -1;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        if (!socket_table[i].in_use) {
            socket_table[i].in_use = 1; socket_table[i].domain = domain;
            socket_table[i].type = type; socket_table[i].protocol = protocol;
            socket_table[i].state = TCP_STATE_CLOSED; socket_table[i].local_ip = 0;
            socket_table[i].local_port = 0; socket_table[i].remote_ip = 0;
            socket_table[i].remote_port = 0;
            return i;
        }
    }
    return -1;
}
int sys_bind(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* s = &socket_table[sockfd];
    if (!s->in_use || addr->sin_family != AF_INET) return -1;
    s->local_ip = addr->sin_addr; s->local_port = ntohs(addr->sin_port);
    return 0;
}
int sys_connect(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* s = &socket_table[sockfd];
    if (!s->in_use || addr->sin_family != AF_INET) return -1;
    if (s->type == SOCK_STREAM) {
        s->state = TCP_STATE_CLOSED;
        klog(LOG_WARNING, "TCP connect rejected: SYN/SYN-ACK/ACK state machine is not implemented");
        return -2;
    }
    s->remote_ip = addr->sin_addr; s->remote_port = ntohs(addr->sin_port);
    return 0;
}
int sys_listen(int sockfd, int backlog) {
    (void)backlog;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use) return -1;
    klog(LOG_WARNING, "TCP listen unavailable until a real TCP receive/accept path is implemented");
    return -2;
}
int sys_send(int sockfd, const void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !buf) return -1;
    socket_entry_t* s = &socket_table[sockfd];
    if (!s->in_use || !s->remote_ip || !s->remote_port) return -1;
    if (s->type == SOCK_STREAM) return -2;
    if (s->type != SOCK_DGRAM || len > 1472u) return -1;
    netdev_t* dev = netdev_get_default();
    if (!dev) return -2;
    return udp_send_packet(dev, s->remote_ip, s->local_port, s->remote_port,
                           (const uint8_t*)buf, len);
}
int sys_recv(int sockfd, void* buf, uint32_t len, int flags) {
    (void)len; (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !buf || !socket_table[sockfd].in_use) return -1;
    klog(LOG_WARNING, "Socket receive unavailable: network protocol dispatch and receive queues are not implemented");
    return -2;
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

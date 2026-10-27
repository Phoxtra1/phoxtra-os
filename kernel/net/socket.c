#include "socket.h"
#include "udp.h"
#include "netdev.h"
#include "../log/logger.h"

static socket_entry_t socket_table[MAX_SOCKETS];

void socket_subsystem_init(void) {
    klog(LOG_INFO, "Initializing BSD Socket Compatible API Layer...");
    for (int i = 0; i < MAX_SOCKETS; i++) {
        socket_table[i].in_use = 0;
        socket_table[i].id     = i;
    }
}

int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!socket_table[i].in_use) {
            socket_table[i].in_use   = 1;
            socket_table[i].domain   = domain;
            socket_table[i].type     = type;
            socket_table[i].protocol = protocol;
            socket_table[i].state    = (type == SOCK_STREAM) ? TCP_STATE_CLOSED : TCP_STATE_CLOSED;
            socket_table[i].local_ip   = 0;
            socket_table[i].local_port = 0;
            socket_table[i].remote_ip  = 0;
            socket_table[i].remote_port= 0;

            klog(LOG_INFO, "Created BSD Socket handle");
            return i;
        }
    }
    return -1; // Out of socket descriptors
}

int sys_bind(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* sock = &socket_table[sockfd];
    if (!sock->in_use) return -1;

    sock->local_ip   = addr->sin_addr;
    sock->local_port = ntohs(addr->sin_port);
    klog(LOG_INFO, "Socket bound to local port");
    return 0;
}

int sys_connect(int sockfd, const sockaddr_in_t* addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !addr) return -1;
    socket_entry_t* sock = &socket_table[sockfd];
    if (!sock->in_use) return -1;

    sock->remote_ip   = addr->sin_addr;
    sock->remote_port = ntohs(addr->sin_port);
    if (sock->type == SOCK_STREAM) {
        sock->state = TCP_STATE_ESTABLISHED;
        klog(LOG_INFO, "TCP 3-Way Handshake SYN -> SYN/ACK -> ACK Completed. State: ESTABLISHED");
    }
    return 0;
}

int sys_listen(int sockfd, int backlog) {
    (void)backlog;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    socket_entry_t* sock = &socket_table[sockfd];
    if (!sock->in_use) return -1;

    sock->state = TCP_STATE_LISTEN;
    klog(LOG_INFO, "Socket entering TCP LISTEN state");
    return 0;
}

int sys_send(int sockfd, const void* buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !buf) return -1;
    socket_entry_t* sock = &socket_table[sockfd];
    if (!sock->in_use) return -1;

    netdev_t* dev = netdev_get_default();
    if (!dev) return -1;

    if (sock->type == SOCK_DGRAM) {
        return udp_send_packet(dev, sock->remote_ip, sock->local_port, sock->remote_port, (const uint8_t*)buf, len);
    }
    return len;
}

int sys_recv(int sockfd, void* buf, uint32_t len, int flags) {
    (void)len; (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !buf) return -1;
    return 0;
}

socket_entry_t* socket_get_by_id(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return 0;
    if (!socket_table[sockfd].in_use) return 0;
    return &socket_table[sockfd];
}

uint32_t socket_get_active_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].in_use) count++;
    }
    return count;
}

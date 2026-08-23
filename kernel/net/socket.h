#ifndef PHOXTRA_SOCKET_H
#define PHOXTRA_SOCKET_H

#include "tcp.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AF_INET     2
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

#define MAX_SOCKETS 32

typedef struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
} sockaddr_in_t;

typedef struct socket_entry {
    int id;
    int domain;
    int type;
    int protocol;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    tcp_state_t state;
    uint8_t in_use;
} socket_entry_t;

void socket_subsystem_init(void);
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const sockaddr_in_t* addr);
int sys_connect(int sockfd, const sockaddr_in_t* addr);
int sys_listen(int sockfd, int backlog);
int sys_send(int sockfd, const void* buf, uint32_t len, int flags);
int sys_recv(int sockfd, void* buf, uint32_t len, int flags);
socket_entry_t* socket_get_by_id(int sockfd);
uint32_t socket_get_active_count(void);

#ifdef __cplusplus
}
#endif

#endif

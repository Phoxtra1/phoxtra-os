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
#define SOCKET_UDP_RECEIVE_CAPACITY 1472u

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

    /* TCP sequence/receive/send state. One unacknowledged data segment is tracked. */
    uint32_t snd_nxt;
    uint32_t snd_una;
    uint32_t rcv_nxt;
    uint8_t tcp_rx_buffer[TCP_RECV_CAPACITY];
    uint32_t tcp_rx_head;
    uint32_t tcp_rx_count;
    uint8_t tcp_tx_buffer[TCP_MSS];
    uint16_t tcp_tx_length;
    uint32_t tcp_tx_seq;
    uint64_t tcp_tx_sent_at;
    uint8_t tcp_tx_retries;
    uint8_t peer_closed;
    int error;

    /* UDP preserves one pending datagram per socket; extra datagrams are dropped. */
    uint8_t udp_rx_buffer[SOCKET_UDP_RECEIVE_CAPACITY];
    uint16_t udp_rx_length;
    uint32_t udp_rx_source_ip;
    uint16_t udp_rx_source_port;
    uint8_t udp_rx_pending;
    uint8_t udp_bound;

    /* Passive-open queue for a listening TCP socket. */
    int parent_id;
    uint8_t listen_backlog;
    int pending_children[TCP_MAX_LISTEN_BACKLOG];
    uint8_t pending_head;
    uint8_t pending_count;
} socket_entry_t;

void socket_subsystem_init(void);
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const sockaddr_in_t* addr);
int sys_connect(int sockfd, const sockaddr_in_t* addr);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, sockaddr_in_t* addr);
int sys_send(int sockfd, const void* buf, uint32_t len, int flags);
int sys_recv(int sockfd, void* buf, uint32_t len, int flags);
int sys_close(int sockfd);
socket_entry_t* socket_get_by_id(int sockfd);
uint32_t socket_get_active_count(void);

#ifdef __cplusplus
}
#endif

#endif

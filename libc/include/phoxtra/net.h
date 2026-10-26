#ifndef PHOXTRA_USER_NET_H
#define PHOXTRA_USER_NET_H

#include <stddef.h>
#include <stdint.h>

#define PHOXTRA_AF_INET 2
#define PHOXTRA_SOCK_STREAM 1
#define PHOXTRA_SOCK_DGRAM 2
#define PHOXTRA_IPPROTO_TCP 6
#define PHOXTRA_IPPROTO_UDP 17

typedef struct phoxtra_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;   /* Network byte order. */
    uint32_t sin_addr;   /* Same byte-oriented IPv4 representation as Phoxtra's kernel API. */
    uint8_t sin_zero[8];
} phoxtra_sockaddr_in_t;

/* Bounded int 0x80 network calls. TCP send/receive are currently single-segment capped. */
int phoxtra_socket(int domain, int type, int protocol);
int phoxtra_connect(int sockfd, const phoxtra_sockaddr_in_t* remote);
int phoxtra_send(int sockfd, const void* buffer, size_t length, int flags);
int phoxtra_recv(int sockfd, void* buffer, size_t length, int flags);
int phoxtra_close(int sockfd);
int phoxtra_dns_resolve_ipv4(const char* hostname, uint32_t* address_out);

static inline uint16_t phoxtra_htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}
static inline uint32_t phoxtra_ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

#endif

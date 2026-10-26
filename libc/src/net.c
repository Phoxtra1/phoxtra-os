#include "../include/phoxtra/net.h"

#define PHOXTRA_SYS_SOCKET      6u
#define PHOXTRA_SYS_CONNECT     7u
#define PHOXTRA_SYS_SEND        8u
#define PHOXTRA_SYS_RECV        9u
#define PHOXTRA_SYS_CLOSE       10u
#define PHOXTRA_SYS_DNS_RESOLVE 11u
#define PHOXTRA_USER_IO_MAX     1460u

static inline int64_t phoxtra_invoke(uint64_t number, uint64_t arg1,
                                     uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    register uint64_t rax __asm__("rax") = number;
    register uint64_t rdi __asm__("rdi") = arg1;
    register uint64_t rsi __asm__("rsi") = arg2;
    register uint64_t rdx __asm__("rdx") = arg3;
    register uint64_t rcx __asm__("rcx") = arg4;
    __asm__ volatile ("int $0x80"
        : "+a"(rax)
        : "D"(rdi), "S"(rsi), "d"(rdx), "c"(rcx)
        : "memory", "cc");
    return (int64_t)rax;
}

int phoxtra_socket(int domain, int type, int protocol) {
    return (int)phoxtra_invoke(PHOXTRA_SYS_SOCKET, (uint64_t)domain,
                               (uint64_t)type, (uint64_t)protocol, 0u);
}

int phoxtra_connect(int sockfd, const phoxtra_sockaddr_in_t* remote) {
    if (!remote) return -14;
    return (int)phoxtra_invoke(PHOXTRA_SYS_CONNECT, (uint64_t)sockfd,
                               (uint64_t)remote, 0u, 0u);
}

int phoxtra_send(int sockfd, const void* buffer, size_t length, int flags) {
    if ((!buffer && length) || length > PHOXTRA_USER_IO_MAX) return -22;
    return (int)phoxtra_invoke(PHOXTRA_SYS_SEND, (uint64_t)sockfd,
                               (uint64_t)buffer, (uint64_t)length, (uint64_t)flags);
}

int phoxtra_recv(int sockfd, void* buffer, size_t length, int flags) {
    if ((!buffer && length) || length > PHOXTRA_USER_IO_MAX) return -22;
    return (int)phoxtra_invoke(PHOXTRA_SYS_RECV, (uint64_t)sockfd,
                               (uint64_t)buffer, (uint64_t)length, (uint64_t)flags);
}

int phoxtra_close(int sockfd) {
    return (int)phoxtra_invoke(PHOXTRA_SYS_CLOSE, (uint64_t)sockfd, 0u, 0u, 0u);
}

int phoxtra_dns_resolve_ipv4(const char* hostname, uint32_t* address_out) {
    if (!hostname || !address_out) return -14;
    int64_t resolved = phoxtra_invoke(PHOXTRA_SYS_DNS_RESOLVE,
                                      (uint64_t)hostname, 0u, 0u, 0u);
    if (resolved <= 0 || (uint64_t)resolved > 0xFFFFFFFFu) return -2;
    *address_out = (uint32_t)resolved;
    return 0;
}

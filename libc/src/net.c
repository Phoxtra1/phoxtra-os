#include "../include/phoxtra/net.h"

#define PHOXTRA_SYS_SOCKET      6u
#define PHOXTRA_SYS_CONNECT     7u
#define PHOXTRA_SYS_SEND        8u
#define PHOXTRA_SYS_RECV        9u
#define PHOXTRA_SYS_CLOSE       10u
#define PHOXTRA_SYS_DNS_RESOLVE 11u
#define PHOXTRA_USER_IO_MAX     1460u

/* Freestanding inet_pton implementation needed for X.509 IP subjectAltName checks. */
static int parse_ipv4_address(const char* src, uint8_t out[4])
{
    if (!src || !*src || !out) return 0;
    const char* p = src;
    for (unsigned part = 0; part < 4u; ++part) {
        unsigned value = 0, digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 3u) return 0;
            value = value * 10u + (unsigned)(*p - '0');
            if (value > 255u) return 0;
            ++p;
        }
        if (!digits) return 0;
        out[part] = (uint8_t)value;
        if (part < 3u) {
            if (*p != '.') return 0;
            ++p;
        } else if (*p != '\0') return 0;
    }
    return 1;
}

static int parse_ipv6_group(const char* begin, size_t length, uint16_t* word)
{
    if (!length || length > 4u || !word) return 0;
    unsigned value = 0;
    for (size_t i = 0; i < length; ++i) {
        const char c = begin[i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A' + 10);
        else return 0;
        value = (value << 4) | digit;
    }
    *word = (uint16_t)value;
    return 1;
}

static int parse_ipv6_side(const char* begin, const char* end,
                           uint16_t words[8], unsigned* count,
                           int allow_ipv4_tail)
{
    const char* p = begin;
    while (p < end) {
        const char* token = p;
        while (p < end && *p != ':') ++p;
        const size_t length = (size_t)(p - token);
        if (!length) return 0;
        int has_dot = 0;
        for (size_t i = 0; i < length; ++i) if (token[i] == '.') has_dot = 1;
        if (has_dot) {
            uint8_t address[4];
            char text[16];
            if (!allow_ipv4_tail || p != end || length >= sizeof(text) || *count > 6u) return 0;
            for (size_t i = 0; i < length; ++i) text[i] = token[i];
            text[length] = '\0';
            if (!parse_ipv4_address(text, address)) return 0;
            words[(*count)++] = (uint16_t)(((uint16_t)address[0] << 8) | address[1]);
            words[(*count)++] = (uint16_t)(((uint16_t)address[2] << 8) | address[3]);
        } else {
            if (*count >= 8u || !parse_ipv6_group(token, length, &words[*count])) return 0;
            ++*count;
        }
        if (p < end) {
            ++p;
            if (p == end) return 0;
        }
    }
    return 1;
}

int inet_pton(int af, const char* src, void* dst)
{
    if (!src || !dst) return 0;
    if (af == 2) {
        uint8_t parsed[4];
        if (!parse_ipv4_address(src, parsed)) return 0;
        uint8_t* out = (uint8_t*)dst;
        for (unsigned i = 0; i < 4u; ++i) out[i] = parsed[i];
        return 1;
    }
    if (af != 10) return -1;

    const size_t source_length = strlen(src);
    const char* double_colon = strstr(src, "::");
    uint16_t words[8] = {0};
    unsigned left_count = 0, right_count = 0;
    if (!double_colon) {
        if (!parse_ipv6_side(src, src + source_length, words, &left_count, 1) ||
            left_count != 8u) return 0;
    } else {
        if (strstr(double_colon + 2, "::")) return 0;
        uint16_t left_words[8] = {0}, right_words[8] = {0};
        if (!parse_ipv6_side(src, double_colon, left_words, &left_count, 0) ||
            !parse_ipv6_side(double_colon + 2, src + source_length, right_words,
                             &right_count, 1)) return 0;
        if (left_count + right_count >= 8u) return 0;
        for (unsigned i = 0; i < left_count; ++i) words[i] = left_words[i];
        for (unsigned i = 0; i < right_count; ++i)
            words[8u - right_count + i] = right_words[i];
    }
    uint8_t* out = (uint8_t*)dst;
    for (unsigned i = 0; i < 8u; ++i) {
        out[2u * i] = (uint8_t)(words[i] >> 8);
        out[2u * i + 1u] = (uint8_t)words[i];
    }
    return 1;
}



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

#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/unistd.h"

/* Keep the user ABI aligned with the kernel's validated int 0x80 entry path. */
#define PHOXTRA_SYS_PRINT 1u
#define PHOXTRA_SYS_EXIT  2u
#define PHOXTRA_SYS_GETPID 5u
#define PHOXTRA_SYS_WRITE 12u

static inline int64_t phoxtra_syscall1(uint64_t sys_num, uint64_t arg1) {
    register uint64_t rax __asm__("rax") = sys_num;
    register uint64_t rdi __asm__("rdi") = arg1;
    __asm__ volatile ("int $0x80"
        : "+a"(rax)
        : "D"(rdi)
        : "memory", "cc");
    return (int64_t)rax;
}

static inline int64_t phoxtra_syscall3(uint64_t sys_num, uint64_t arg1,
                                       uint64_t arg2, uint64_t arg3) {
    register uint64_t rax __asm__("rax") = sys_num;
    register uint64_t rdi __asm__("rdi") = arg1;
    register uint64_t rsi __asm__("rsi") = arg2;
    register uint64_t rdx __asm__("rdx") = arg3;
    __asm__ volatile ("int $0x80"
        : "+a"(rax)
        : "D"(rdi), "S"(rsi), "d"(rdx)
        : "memory", "cc");
    return (int64_t)rax;
}

size_t strlen(const char* s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src) return dest;
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0;
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int puts(const char* str) {
    if (!str) return -1;
    size_t length = strlen(str);
    if (length > 1023u) return -22;
    int result = write(1, str, length);
    if (result < 0) return result;
    int newline = write(1, "\n", 1);
    if (newline < 0) return newline;
    return result + newline;
}
int write(int fd, const void* buf, size_t count) {
    if (!buf && count) return -14;
    if (count > 0xFFFFFFFFu) return -22;
    return (int)phoxtra_syscall3(PHOXTRA_SYS_WRITE, (uint64_t)fd,
                                 (uint64_t)buf, (uint64_t)count);
}

int getpid(void) {
    return (int)phoxtra_syscall1(PHOXTRA_SYS_GETPID, 0);
}

void exit(int status) {
    phoxtra_syscall1(PHOXTRA_SYS_EXIT, (uint64_t)status);
    while (1) { }
}

#ifndef PHOXTRA_LIBC_STRING_H
#define PHOXTRA_LIBC_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* lhs, const void* rhs, size_t n);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
void* memmove(void* dest, const void* src, size_t n);
void explicit_bzero(void* s, size_t n);

#ifdef __cplusplus
}
#endif

#endif

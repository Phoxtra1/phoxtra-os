#ifndef PHOXTRA_LIBC_STDLIB_H
#define PHOXTRA_LIBC_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void exit(int status);
int atoi(const char* str);
int abs(int j);

#ifdef __cplusplus
}
#endif

#endif

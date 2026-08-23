#ifndef PHOXTRA_LIBC_STDIO_H
#define PHOXTRA_LIBC_STDIO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE FILE;

int printf(const char* format, ...);
int puts(const char* str);
int putchar(int c);
int getchar(void);

FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);

#ifdef __cplusplus
}
#endif

#endif

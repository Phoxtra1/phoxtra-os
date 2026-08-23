#ifndef PHOXTRA_LIBC_UNISTD_H
#define PHOXTRA_LIBC_UNISTD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int read(int fd, void* buf, size_t count);
int write(int fd, const void* buf, size_t count);
int open(const char* pathname, int flags);
int close(int fd);
int getpid(void);
unsigned int usleep(unsigned int usec);
int fork(void);
int execve(const char* filename, char* const argv[], char* const envp[]);

#ifdef __cplusplus
}
#endif

#endif

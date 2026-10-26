#ifndef PHOXTRA_SYSCALL_H
#define PHOXTRA_SYSCALL_H

#include <stdint.h>

/* System call numbers: int 0x80, number in RAX and arguments in RDI/RSI/RDX/RCX. */
#define SYS_PRINT       1
#define SYS_EXIT        2
#define SYS_YIELD       3
#define SYS_SLEEP       4
#define SYS_GETPID      5
#define SYS_SOCKET      6
#define SYS_CONNECT     7
#define SYS_SEND        8
#define SYS_RECV        9
#define SYS_CLOSE       10
#define SYS_DNS_RESOLVE 11
#define SYS_WRITE       12

#ifdef __cplusplus
extern "C" {
#endif

struct registers;  /* Forward declaration from irq.h */

void syscall_init(void);
void syscall_handler(struct registers* regs);

#ifdef __cplusplus
}
#endif
#endif

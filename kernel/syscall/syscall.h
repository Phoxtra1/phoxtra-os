#ifndef PHOXTRA_SYSCALL_H
#define PHOXTRA_SYSCALL_H

#include <stdint.h>

/* System call numbers */
#define SYS_PRINT   1
#define SYS_EXIT    2
#define SYS_YIELD   3
#define SYS_SLEEP   4
#define SYS_GETPID  5

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

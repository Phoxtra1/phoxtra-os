#ifndef PHOXTRA_IRQ_H
#define PHOXTRA_IRQ_H

#include <stdint.h>

struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct registers* regs);

void irq_init(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);
void irq_set_mask(unsigned char irq_line);
void irq_clear_mask(unsigned char irq_line);

#endif

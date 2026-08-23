#include "irq.h"
#include "pic.h"
#include "panic.hpp"

#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1

static irq_handler_t irq_handlers[16] = {0};

static const char* exception_names[32] = {
    "Divide Error",
    "Debug Exception",
    "Non-Maskable Interrupt",
    "Breakpoint Exception",
    "Overflow Exception",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved Exception",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved Exception",
    "Reserved Exception",
    "Reserved Exception",
    "Reserved Exception",
    "Reserved Exception",
    "Reserved Exception",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved Exception"
};

static inline void outb(unsigned short port, unsigned char value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void irq_set_mask(unsigned char irq_line) {
    uint16_t port;
    uint8_t value;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = inb(port) | (1 << irq_line);
    outb(port, value);
}

void irq_clear_mask(unsigned char irq_line) {
    uint16_t port;
    uint8_t value;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = inb(port) & ~(1 << irq_line);
    outb(port, value);
}

void irq_init(void) {
    // Remap IRQ 0-15 to vectors 0x20-0x2F
    pic_remap(0x20, 0x28);

    // Mask all IRQs by default except Cascade IRQ2 on Master PIC
    outb(PIC1_DATA, 0xFB);
    outb(PIC2_DATA, 0xFF);
}

void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
        irq_clear_mask(irq);
    }
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = 0;
        irq_set_mask(irq);
    }
}

// Common interrupt dispatcher called from interrupts.asm
void interrupt_handler(struct registers* regs) {
    if (regs->int_no < 32) {
        // CPU Exception
        kernel_panic(exception_names[regs->int_no], regs);
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        // Hardware IRQ
        int irq = regs->int_no - 32;
        irq_handler_t handler = irq_handlers[irq];
        if (handler != 0) {
            handler(regs);
        }
        pic_send_eoi(irq);
    }
}

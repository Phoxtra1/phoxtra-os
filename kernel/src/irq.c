#include "irq.h"
#include "pic.h"
#include "panic.hpp"
#include "../syscall/syscall.h"
#include "../log/logger.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"

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

/* User-triggerable synchronous faults must not panic the whole kernel. */
static int exception_is_process_local(uint64_t vector) {
    switch (vector) {
        case 0:  /* Divide error */
        case 3:  /* Breakpoint */
        case 4:  /* Overflow */
        case 5:  /* Bounds exceeded */
        case 6:  /* Invalid opcode */
        case 7:  /* Device not available */
        case 13: /* General protection fault */
        case 14: /* Page fault */
        case 16: /* x87 floating-point exception */
        case 17: /* Alignment check */
        case 19: /* SIMD floating-point exception */
        case 21: /* Control protection exception */
            return 1;
        default:
            return 0; /* NMI, double fault, machine check, and unknown traps stay fatal. */
    }
}

static void log_fault_value(const char* label, uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char line[96];
    unsigned length = 0;
    while (label[length] && length < 60u) {
        line[length] = label[length];
        ++length;
    }
    const char separator[] = " = 0x";
    for (unsigned i = 0; i < sizeof(separator) - 1u; ++i)
        line[length++] = separator[i];
    for (int shift = 60; shift >= 0; shift -= 4)
        line[length++] = digits[(value >> (unsigned)shift) & 0xFu];
    line[length] = '\0';
    klog(LOG_ERROR, line);
}

/* User-triggerable synchronous faults include context useful for debugging
 * a failed guest ABI call without turning the process fault into a panic. */
static int terminate_faulting_user_process(struct registers* regs) {
    if (!regs || (regs->cs & 3u) != 3u ||
        !exception_is_process_local(regs->int_no)) return 0;

    pcb_t* current = scheduler_current();
    if (!current || !current->user_mode || !current->page_directory ||
        current->pid == 0xFFFFFFFFu) return 0;

    if (regs->int_no == 14)
        klog(LOG_ERROR, "PHOXTRA_USER_PAGE_FAULT_ISOLATED");
    else
        klog(LOG_ERROR, "PHOXTRA_USER_EXCEPTION_ISOLATED");

    log_fault_value("Faulting process PID", current->pid);
    klog(LOG_ERROR, current->name);
    log_fault_value("User exception vector", regs->int_no);
    log_fault_value("User exception error code", regs->err_code);
    log_fault_value("User exception RIP", regs->rip);
    log_fault_value("User exception CS", regs->cs);
    log_fault_value("User exception RSP", regs->rsp);
    if (regs->int_no == 14u) {
        uint64_t fault_address;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
        log_fault_value("User page-fault address", fault_address);
    }

    /* pcb_destroy defers reclamation while this process's CR3 is active. */
    process_destroy(current->pid);
    scheduler_yield();
    return 1;
}

// Common interrupt dispatcher called from interrupts.asm
void interrupt_handler(struct registers* regs) {
    if (regs->int_no < 32) {
        /* An invalid Ring 3 access kills only the offending process. */
        if (terminate_faulting_user_process(regs)) return;
        kernel_panic(exception_names[regs->int_no], regs);
    } else if (regs->int_no == 0x80) {
        syscall_handler(regs);
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        // Hardware IRQ
        int irq = regs->int_no - 32;
        irq_handler_t handler = irq_handlers[irq];

        /*
         * IRQ0 may context-switch inside scheduler_tick(). A task switch
         * suspends this interrupt frame, so an EOI sent after the callback
         * would remain outstanding until the old task is scheduled again.
         * A PIC IRQ still in service blocks subsequent timer ticks, preventing
         * sleeping/yielded processes from being scheduled. Acknowledge the
         * PIT before its callback is allowed to switch tasks.
         */
        if (irq == 0) pic_send_eoi((unsigned char)irq);
        if (handler != 0) handler(regs);
        if (irq != 0) pic_send_eoi((unsigned char)irq);
    }
}

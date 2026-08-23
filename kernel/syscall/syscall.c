#include "syscall.h"
#include "../arch/x86_64/idt.h"
#include "../include/irq.h"
#include "../log/logger.h"
#include "../scheduler/scheduler.h"

/* ────────────────────────────────────────────────
   Individual Syscall Handlers
   ──────────────────────────────────────────────── */
static void sys_print(struct registers* regs) {
    /* rsi holds the pointer to the string to print (convention) */
    (void)regs;
    klog(LOG_INFO, "[SYS_PRINT] syscall invoked");
}

static void sys_exit(struct registers* regs) {
    (void)regs;
    klog(LOG_INFO, "[SYS_EXIT] process exit requested");
    /* TODO: terminate calling process when process-context integration is complete */
}

static void sys_yield(struct registers* regs) {
    (void)regs;
    klog(LOG_INFO, "[SYS_YIELD] voluntary yield");
    scheduler_yield();
}

static void sys_sleep(struct registers* regs) {
    (void)regs;
    klog(LOG_INFO, "[SYS_SLEEP] sleep requested");
    /* TODO: implement sleep queue when process states are fully wired */
}

static void sys_getpid(struct registers* regs) {
    (void)regs;
    pcb_t* current = scheduler_current();
    uint32_t pid = current ? current->pid : 0;
    /* Return value placed in rax (standard System V convention) */
    if (regs) {
        /* regs->rax = pid;  — uncomment once regs is fully linked to irq_regs */
        (void)pid;
    }
    klog(LOG_INFO, "[SYS_GETPID] pid returned");
}

/* ────────────────────────────────────────────────
   Dispatcher Table
   ──────────────────────────────────────────────── */
typedef void (*syscall_fn_t)(struct registers*);

static syscall_fn_t syscall_table[] = {
    (void*)0,       /* 0 — reserved */
    sys_print,      /* 1 — SYS_PRINT  */
    sys_exit,       /* 2 — SYS_EXIT   */
    sys_yield,      /* 3 — SYS_YIELD  */
    sys_sleep,      /* 4 — SYS_SLEEP  */
    sys_getpid,     /* 5 — SYS_GETPID */
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

/* ────────────────────────────────────────────────
   Main Syscall Handler — invoked from IDT gate 0x80
   ──────────────────────────────────────────────── */
void syscall_handler(struct registers* regs) {
    /* Syscall number passed in rax */
    uint64_t num = regs ? regs->rax : 0;

    if (num > 0 && num < SYSCALL_TABLE_SIZE && syscall_table[num]) {
        syscall_table[num](regs);
    } else {
        klog(LOG_WARNING, "syscall: invalid syscall number");
    }
}

/* ────────────────────────────────────────────────
   Initialization — register IDT gate 0x80
   ──────────────────────────────────────────────── */

/* ISR stub for int 0x80 is already in isr.asm; install it as an IRQ handler
   via the generic IRQ dispatcher on vector 128 (0x80) using irq_install_handler.
   Since vectors 0x20-0x2F are hardware IRQs, int 0x80 routes through the
   generic ISR path; we hook it at the high-level handler stage.              */
#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

extern void syscall_entry(void);

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static irq_handler_t old_handler = (void*)0;

static void syscall_irq_bridge(struct registers* regs) {
    (void)old_handler;
    syscall_handler(regs);
}

void syscall_init(void) {
    /* Register handler for software interrupt int 0x80 */
    irq_install_handler(128 - 32, syscall_irq_bridge);

    /* Configure Model Specific Registers (MSR) for fast hardware syscall/sysret */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 0x1); // SCE: System Call Enable
    wrmsr(MSR_STAR, ((uint64_t)0x0013 << 48) | ((uint64_t)0x0008 << 32));
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_FMASK, 0x200); // Clear IF flag during syscall execution

    klog(LOG_KERNEL, "Syscall framework initialized (int 0x80 & syscall MSR)");
}


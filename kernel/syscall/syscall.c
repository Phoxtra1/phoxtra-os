#include "syscall.h"
#include "../include/irq.h"
#include "../log/logger.h"
#include "../scheduler/scheduler.h"
#include "../process/process.h"
#include "../memory/vmm.h"

#define USER_PRINT_MAX 256
#define SYS_ERROR_INVALID (-22)
#define SYS_ERROR_FAULT   (-14)

static void sys_print(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!regs || !current || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    char message[USER_PRINT_MAX + 1];
    size_t i = 0;
    for (; i < USER_PRINT_MAX; ++i) {
        if (vmm_copy_from_user(current->page_directory, &message[i], regs->rdi + i, 1) != 0) {
            klog(LOG_WARNING, "SYS_PRINT rejected inaccessible user pointer");
            regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
            return;
        }
        if (!message[i]) break;
    }
    message[USER_PRINT_MAX] = '\0';
    if (i == USER_PRINT_MAX) message[USER_PRINT_MAX] = '\0';
    klog(LOG_INFO, message);
    regs->rax = (uint64_t)i;
}

static void sys_exit(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!regs || !current || current->pid == 0xFFFFFFFFu || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint32_t pid = current->pid;
    klog(LOG_INFO, "User process exited via SYS_EXIT");
    process_destroy(pid);
    regs->rax = 0;
    scheduler_yield();
}

static void sys_yield(struct registers* regs) {
    if (!regs || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = 0;
    scheduler_yield();
}

static void sys_sleep(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!regs || !current || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t ticks = regs->rdi;
    if (ticks > 0x7FFFFFFFFFFFFFFFULL - scheduler_get_uptime_ticks()) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    current->wake_tick = scheduler_get_uptime_ticks() + (ticks ? ticks : 1);
    current->state = PROCESS_STATE_SLEEPING;
    regs->rax = 0;
    scheduler_yield();
}

static void sys_getpid(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!regs || !current || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = current->pid;
}

typedef void (*syscall_fn_t)(struct registers*);
static syscall_fn_t syscall_table[] = {
    (void*)0, sys_print, sys_exit, sys_yield, sys_sleep, sys_getpid
};
#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_handler(struct registers* regs) {
    if (!regs) return;
    uint64_t num = regs->rax;
    if ((regs->cs & 3u) != 3u) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (num > 0 && num < SYSCALL_TABLE_SIZE && syscall_table[num])
        syscall_table[num](regs);
    else
        regs->rax = (uint64_t)(int64_t)-38;
}

/* int 0x80 uses the interrupt gate so the CPU switches to TSS.RSP0 before
   entering C. The fast SYSCALL MSR path stays disabled until its dedicated
   stack-switching entry/return path has been independently validated. */
void syscall_init(void) {
    klog(LOG_KERNEL, "Syscall framework initialized (Ring 3 int 0x80)");
}

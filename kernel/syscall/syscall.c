#include "syscall.h"
#include "../include/irq.h"
#include "../log/logger.h"
#include "../scheduler/scheduler.h"
#include "../process/process.h"
#include "../memory/vmm.h"
#include "../net/socket.h"
#include "../net/netdev.h"
#include "../net/dns.h"
#include "../security/random.h"
#include "../services/rtc.h"
#include <stddef.h>
#include <stdint.h>

#define USER_PRINT_MAX 256u
#define USER_HOST_MAX 254u
#define USER_SOCKET_IO_MAX TCP_MSS
#define USER_WRITE_MAX 1024u
#define USER_RANDOM_MAX 256u
#define SYS_ERROR_INVALID (-22)
#define SYS_ERROR_FAULT   (-14)
#define SYS_ERROR_BAD_FD  (-9)
#define SYS_ERROR_NOSYS   (-38)

extern void console_put_char(char ch);

static int user_call_valid(const struct registers* regs, const pcb_t* current) {
    return regs && current && (regs->cs & 3u) == 3u;
}

static void sys_print_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    char message[USER_PRINT_MAX + 1u];
    size_t i = 0;
    for (; i < USER_PRINT_MAX; ++i) {
        if (vmm_copy_from_user(current->page_directory, &message[i], regs->rdi + i, 1u) != 0) {
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

static void sys_write_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (regs->rdi != 1u && regs->rdi != 2u) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_BAD_FD;
        return;
    }
    uint64_t requested = regs->rdx;
    if (requested > USER_WRITE_MAX) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (!requested) {
        regs->rax = 0;
        return;
    }

    uint8_t bytes[USER_WRITE_MAX];
    if (vmm_copy_from_user(current->page_directory, bytes, regs->rsi, (size_t)requested) != 0) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    for (uint32_t i = 0; i < (uint32_t)requested; ++i) console_put_char((char)bytes[i]);
    regs->rax = requested;
}

static void sys_exit_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current) || current->pid == 0xFFFFFFFFu) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint32_t pid = current->pid;
    klog(LOG_INFO, "User process exited via SYS_EXIT");
    process_destroy(pid);
    regs->rax = 0;
    scheduler_yield();
}

static void sys_yield_user(struct registers* regs) {
    if (!regs || (regs->cs & 3u) != 3u) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = 0;
    scheduler_yield();
    klog(LOG_INFO, "Regression trace: SYS_YIELD resumed");
}

static void sys_sleep_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t ticks = regs->rdi;
    if (ticks > 0x7FFFFFFFFFFFFFFFULL - scheduler_get_uptime_ticks()) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    current->wake_tick = scheduler_get_uptime_ticks() + (ticks ? ticks : 1u);
    current->state = PROCESS_STATE_SLEEPING;
    regs->rax = 0;
    scheduler_yield();
}

static void sys_getpid_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = current->pid;
}

static void sys_socket_user(struct registers* regs) {
    if (!user_call_valid(regs, scheduler_current())) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = (uint64_t)(int64_t)sys_socket((int)regs->rdi, (int)regs->rsi, (int)regs->rdx);
}

static void sys_connect_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    sockaddr_in_t remote;
    if (!regs->rsi ||
        vmm_copy_from_user(current->page_directory, &remote, regs->rsi, sizeof(remote)) != 0) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    regs->rax = (uint64_t)(int64_t)sys_connect((int)regs->rdi, &remote);
}

static void sys_send_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t requested = regs->rdx;
    if (requested > USER_SOCKET_IO_MAX) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint8_t bytes[USER_SOCKET_IO_MAX];
    if (requested && (!regs->rsi ||
        vmm_copy_from_user(current->page_directory, bytes, regs->rsi, (size_t)requested) != 0)) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    regs->rax = (uint64_t)(int64_t)sys_send((int)regs->rdi,
        requested ? bytes : (const void*)0, (uint32_t)requested, (int)regs->rcx);
}

static void sys_recv_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t requested = regs->rdx;
    if (requested > USER_SOCKET_IO_MAX) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (requested && !regs->rsi) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    uint8_t bytes[USER_SOCKET_IO_MAX];
    int received = sys_recv((int)regs->rdi, requested ? bytes : (void*)0,
                            (uint32_t)requested, (int)regs->rcx);
    if (received > 0) {
        if ((uint64_t)received > requested ||
            vmm_copy_to_user(current->page_directory, regs->rsi, bytes, (size_t)received) != 0) {
            regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
            return;
        }
    }
    regs->rax = (uint64_t)(int64_t)received;
}

static void sys_close_user(struct registers* regs) {
    if (!user_call_valid(regs, scheduler_current())) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    regs->rax = (uint64_t)(int64_t)sys_close((int)regs->rdi);
}

static void sys_dns_resolve_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (!regs->rdi) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    char hostname[USER_HOST_MAX];
    size_t length = 0;
    for (; length < sizeof(hostname); ++length) {
        if (regs->rdi > UINT64_MAX - length ||
            vmm_copy_from_user(current->page_directory, &hostname[length],
                               regs->rdi + length, 1u) != 0) {
            regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
            return;
        }
        if (!hostname[length]) break;
    }
    if (length == sizeof(hostname)) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    netdev_t* dev = netdev_get_default();
    uint32_t resolved = dns_resolve_hostname(dev, hostname);
    regs->rax = resolved;
}


static void sys_random_user(struct registers* regs) {
    pcb_t* current = scheduler_current();
    if (!user_call_valid(regs, current)) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t requested = regs->rsi;
    if (requested > USER_RANDOM_MAX) { regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID; return; }
    if (!requested) { regs->rax = 0; return; }
    if (!regs->rdi) { regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT; return; }
    uint8_t bytes[USER_RANDOM_MAX];
    if (phoxtra_hw_random(bytes, (size_t)requested) != 0) {
        for (uint32_t i = 0; i < USER_RANDOM_MAX; ++i) bytes[i] = 0;
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_NOSYS;
        return;
    }
    if (vmm_copy_to_user(current->page_directory, regs->rdi, bytes, (size_t)requested) != 0) {
        for (uint32_t i = 0; i < USER_RANDOM_MAX; ++i) bytes[i] = 0;
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_FAULT;
        return;
    }
    for (uint32_t i = 0; i < USER_RANDOM_MAX; ++i) bytes[i] = 0;
    regs->rax = requested;
}
static void sys_time_user(struct registers* regs) {
    if (!user_call_valid(regs, scheduler_current())) {
        if (regs) regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    uint64_t unix_seconds = 0;
    if (rtc_get_unix_time(&unix_seconds) != 0 || !unix_seconds) {
        regs->rax = (uint64_t)(int64_t)-5;
        return;
    }
    regs->rax = unix_seconds;
}

typedef void (*syscall_fn_t)(struct registers*);
static syscall_fn_t syscall_table[] = {
    (void*)0,
    sys_print_user,
    sys_exit_user,
    sys_yield_user,
    sys_sleep_user,
    sys_getpid_user,
    sys_socket_user,
    sys_connect_user,
    sys_send_user,
    sys_recv_user,
    sys_close_user,
    sys_dns_resolve_user,
    sys_write_user,
    sys_random_user,
    sys_time_user
};
#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_handler(struct registers* regs) {
    if (!regs) return;
    uint64_t num = regs->rax;
    if (num >= SYS_GETPID && num <= SYS_PRINT) {
        if (num == SYS_GETPID) klog(LOG_INFO, "Regression trace: SYS_GETPID entered");
        else if (num == SYS_YIELD) klog(LOG_INFO, "Regression trace: SYS_YIELD entered");
        else if (num == SYS_SLEEP) klog(LOG_INFO, "Regression trace: SYS_SLEEP entered");
        else if (num == SYS_PRINT) klog(LOG_INFO, "Regression trace: SYS_PRINT entered");
        else if (num == SYS_EXIT) klog(LOG_INFO, "Regression trace: SYS_EXIT entered");
    }
    if ((regs->cs & 3u) != 3u) {
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_INVALID;
        return;
    }
    if (num > 0 && num < SYSCALL_TABLE_SIZE && syscall_table[num])
        syscall_table[num](regs);
    else
        regs->rax = (uint64_t)(int64_t)SYS_ERROR_NOSYS;
}

/* The supported user ABI is the validated int 0x80 path. Fast SYSCALL MSRs remain disabled. */
void syscall_init(void) {
    klog(LOG_KERNEL, "Syscall framework initialized (Ring 3 int 0x80; bounded network API)");
}

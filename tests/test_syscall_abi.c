#include "../kernel/syscall/syscall.h"
#include "../kernel/include/irq.h"
#include "../kernel/process/pcb.h"
#include "../kernel/log/logger.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TEST_USER_ROOT 0x12345000ULL
#define TEST_USER_BASE 0x0000000100000000ULL
#define TEST_USER_MEMORY_SIZE 128u

static pcb_t mock_process;
static uint8_t mock_user_memory[TEST_USER_MEMORY_SIZE];
static uint64_t mock_uptime_ticks;
static unsigned mock_yield_calls;
static unsigned mock_destroy_calls;
static uint32_t mock_destroyed_pid;
static char last_log[320];

pcb_t* scheduler_current(void) {
    return &mock_process;
}

uint64_t scheduler_get_uptime_ticks(void) {
    return mock_uptime_ticks;
}

void scheduler_yield(void) {
    ++mock_yield_calls;
}

void process_destroy(uint32_t pid) {
    ++mock_destroy_calls;
    mock_destroyed_pid = pid;
    mock_process.state = PROCESS_STATE_TERMINATED;
}

int vmm_copy_from_user(uint64_t pml4_phys, void* kernel_dest,
                       uint64_t user_src, size_t length) {
    if (pml4_phys != TEST_USER_ROOT || user_src < TEST_USER_BASE) return -1;
    uint64_t offset = user_src - TEST_USER_BASE;
    if (offset > TEST_USER_MEMORY_SIZE ||
        length > TEST_USER_MEMORY_SIZE - (size_t)offset) return -1;
    memcpy(kernel_dest, mock_user_memory + (size_t)offset, length);
    return 0;
}

void klog(log_level_t level, const char* message) {
    (void)level;
    if (!message) message = "";
    size_t n = strlen(message);
    if (n >= sizeof(last_log)) n = sizeof(last_log) - 1u;
    memcpy(last_log, message, n);
    last_log[n] = '\0';
}

static void reset_mock(void) {
    memset(&mock_process, 0, sizeof(mock_process));
    memset(mock_user_memory, 0, sizeof(mock_user_memory));
    memset(last_log, 0, sizeof(last_log));
    mock_process.pid = 42;
    mock_process.page_directory = TEST_USER_ROOT;
    mock_process.state = PROCESS_STATE_RUNNING;
    mock_uptime_ticks = 100;
    mock_yield_calls = 0;
    mock_destroy_calls = 0;
    mock_destroyed_pid = 0;
}

#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

static void init_user_regs(struct registers* regs, uint64_t number) {
    memset(regs, 0, sizeof(*regs));
    regs->cs = 0x23; /* user code selector, RPL 3 */
    regs->rax = number;
}

int main(void) {
    struct registers regs;
    reset_mock();
    init_user_regs(&regs, SYS_GETPID);
    syscall_handler(&regs);
    CHECK("SYS_GETPID returns current PID in RAX", regs.rax == 42);

    reset_mock();
    memcpy(mock_user_memory, "hello", sizeof("hello"));
    init_user_regs(&regs, SYS_PRINT);
    regs.rdi = TEST_USER_BASE;
    syscall_handler(&regs);
    CHECK("SYS_PRINT reads its pointer from RDI", regs.rdi == TEST_USER_BASE);
    CHECK("SYS_PRINT returns payload length in RAX", regs.rax == 5);
    CHECK("SYS_PRINT logs the copied user string", strcmp(last_log, "hello") == 0);

    reset_mock();
    init_user_regs(&regs, SYS_PRINT);
    regs.rdi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE + 1u;
    syscall_handler(&regs);
    CHECK("SYS_PRINT rejects an invalid user pointer with -EFAULT",
          (int64_t)regs.rax == -14);

    reset_mock();
    init_user_regs(&regs, SYS_GETPID);
    regs.cs = 0x08; /* supervisor code selector */
    syscall_handler(&regs);
    CHECK("syscalls reject a supervisor-originated register frame",
          (int64_t)regs.rax == -22);

    reset_mock();
    init_user_regs(&regs, 0xFFFFu);
    syscall_handler(&regs);
    CHECK("unknown syscall numbers return -ENOSYS", (int64_t)regs.rax == -38);

    reset_mock();
    init_user_regs(&regs, SYS_YIELD);
    syscall_handler(&regs);
    CHECK("SYS_YIELD returns zero", regs.rax == 0);
    CHECK("SYS_YIELD reaches the scheduler", mock_yield_calls == 1);

    reset_mock();
    init_user_regs(&regs, SYS_SLEEP);
    regs.rdi = 4;
    syscall_handler(&regs);
    CHECK("SYS_SLEEP accepts its duration in RDI", mock_process.wake_tick == 104);
    CHECK("SYS_SLEEP transitions the process to SLEEPING",
          mock_process.state == PROCESS_STATE_SLEEPING);
    CHECK("SYS_SLEEP returns zero and yields", regs.rax == 0 && mock_yield_calls == 1);

    reset_mock();
    mock_uptime_ticks = 200;
    init_user_regs(&regs, SYS_SLEEP);
    regs.rdi = 0;
    syscall_handler(&regs);
    CHECK("zero-duration SYS_SLEEP yields for at least one tick",
          mock_process.wake_tick == 201 && mock_process.state == PROCESS_STATE_SLEEPING);

    reset_mock();
    mock_uptime_ticks = 0x7FFFFFFFFFFFFFFEULL;
    init_user_regs(&regs, SYS_SLEEP);
    regs.rdi = 2;
    syscall_handler(&regs);
    CHECK("SYS_SLEEP rejects tick overflow", (int64_t)regs.rax == -22);
    CHECK("overflowing SYS_SLEEP does not alter scheduler state",
          mock_process.state == PROCESS_STATE_RUNNING && mock_yield_calls == 0);

    reset_mock();
    init_user_regs(&regs, SYS_EXIT);
    regs.rdi = 0;
    syscall_handler(&regs);
    CHECK("SYS_EXIT returns status zero in its saved frame", regs.rax == 0);
    CHECK("SYS_EXIT destroys the calling PID", mock_destroy_calls == 1 && mock_destroyed_pid == 42);
    CHECK("SYS_EXIT marks termination and yields",
          mock_process.state == PROCESS_STATE_TERMINATED && mock_yield_calls == 1);

    puts("Syscall ABI tests passed.");
    return 0;
}

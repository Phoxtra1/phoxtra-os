#include "../kernel/syscall/syscall.h"
#include "../kernel/include/irq.h"
#include "../kernel/process/pcb.h"
#include "../kernel/log/logger.h"
#include "../kernel/net/socket.h"
#include "../kernel/net/netdev.h"
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
static char mock_console[256];
static size_t mock_console_length;
static unsigned mock_socket_calls;
static unsigned mock_connect_calls;
static unsigned mock_send_calls;
static unsigned mock_recv_calls;
static unsigned mock_close_calls;
static unsigned mock_dns_calls;
static int mock_socket_result;
static int mock_connect_result;
static int mock_send_result_mode;
static int mock_close_result;
static int mock_recv_result;
static sockaddr_in_t mock_connected_address;
static uint8_t mock_sent_bytes[1460];
static uint32_t mock_sent_length;
static int mock_sent_fd, mock_sent_flags;
static int mock_received_fd, mock_received_flags;
static uint32_t mock_dns_result;
static int mock_random_failure;
static int mock_rtc_failure;
static char mock_dns_name[254];
static netdev_t mock_netdev;
static int mock_no_current;

pcb_t* scheduler_current(void) { return mock_no_current ? NULL : &mock_process; }
uint64_t scheduler_get_uptime_ticks(void) { return mock_uptime_ticks; }
void scheduler_yield(void) { ++mock_yield_calls; }
void process_destroy(uint32_t pid) {
    ++mock_destroy_calls; mock_destroyed_pid = pid;
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
int vmm_copy_to_user(uint64_t pml4_phys, uint64_t user_dest,
                     const void* kernel_src, size_t length) {
    if (pml4_phys != TEST_USER_ROOT || user_dest < TEST_USER_BASE) return -1;
    uint64_t offset = user_dest - TEST_USER_BASE;
    if (offset > TEST_USER_MEMORY_SIZE ||
        length > TEST_USER_MEMORY_SIZE - (size_t)offset) return -1;
    memcpy(mock_user_memory + (size_t)offset, kernel_src, length);
    return 0;
}
void klog(log_level_t level, const char* message) {
    (void)level;
    if (!message) message = "";
    size_t n = strlen(message);
    if (n >= sizeof(last_log)) n = sizeof(last_log) - 1u;
    memcpy(last_log, message, n); last_log[n] = '\0';
}
void console_put_char(char ch) {
    if (mock_console_length + 1u < sizeof(mock_console)) {
        mock_console[mock_console_length++] = ch;
        mock_console[mock_console_length] = '\0';
    }
}

int sys_socket(int domain, int type, int protocol) {
    ++mock_socket_calls;
    if (domain != AF_INET || type != SOCK_STREAM || protocol != 6) return -22;
    return mock_socket_result;
}
int sys_connect(int sockfd, const sockaddr_in_t* remote) {
    ++mock_connect_calls;
    if (sockfd != mock_socket_result || !remote) return -22;
    mock_connected_address = *remote;
    return mock_connect_result;
}
int sys_send(int sockfd, const void* buf, uint32_t len, int flags) {
    ++mock_send_calls;
    if (sockfd < 0 || (!buf && len) || len > sizeof(mock_sent_bytes)) return -22;
    mock_sent_fd = sockfd; mock_sent_flags = flags; mock_sent_length = len;
    if (len) memcpy(mock_sent_bytes, buf, len);
    return mock_send_result_mode ? mock_send_result_mode : (int)len;
}
int sys_recv(int sockfd, void* buf, uint32_t len, int flags) {
    static const uint8_t response[] = "TLS reply";
    ++mock_recv_calls; mock_received_fd = sockfd; mock_received_flags = flags;
    if (len < sizeof(response) - 1u) return -22;
    memcpy(buf, response, sizeof(response) - 1u);
    mock_recv_result = (int)(sizeof(response) - 1u);
    return mock_recv_result;
}
int sys_close(int sockfd) { ++mock_close_calls; return sockfd == mock_socket_result ? mock_close_result : -9; }
netdev_t* netdev_get_default(void) { return &mock_netdev; }
uint32_t dns_resolve_hostname(netdev_t* dev, const char* hostname) {
    ++mock_dns_calls;
    if (dev != &mock_netdev || !hostname) return 0;
    strncpy(mock_dns_name, hostname, sizeof(mock_dns_name) - 1u);
    mock_dns_name[sizeof(mock_dns_name) - 1u] = '\0';
    return strcmp(hostname, "example.com") == 0 ? mock_dns_result : 0;
}


int phoxtra_hw_random(uint8_t* output, size_t length) {
    if (mock_random_failure || (!output && length)) return -1;
    for (size_t i=0;i<length;++i) output[i]=(uint8_t)(0xA0u+(i&0x0Fu));
    return 0;
}
int rtc_get_unix_time(uint64_t* output) {
    if (mock_rtc_failure || !output) return -1;
    *output=1700000000ULL;
    return 0;
}
static void reset_mock(void) {
    memset(&mock_process, 0, sizeof(mock_process));
    memset(mock_user_memory, 0, sizeof(mock_user_memory));
    memset(last_log, 0, sizeof(last_log));
    memset(mock_console, 0, sizeof(mock_console));
    memset(&mock_connected_address, 0, sizeof(mock_connected_address));
    memset(mock_sent_bytes, 0, sizeof(mock_sent_bytes));
    memset(mock_dns_name, 0, sizeof(mock_dns_name));
    memset(&mock_netdev, 0, sizeof(mock_netdev));
    mock_process.pid = 42;
    mock_process.page_directory = TEST_USER_ROOT;
    mock_process.user_mode = 1;
    mock_process.state = PROCESS_STATE_RUNNING;
    mock_no_current = 0;
    mock_uptime_ticks = 100;
    mock_yield_calls = mock_destroy_calls = mock_socket_calls = mock_connect_calls = 0;
    mock_send_calls = mock_recv_calls = mock_close_calls = mock_dns_calls = 0;
    mock_destroyed_pid = 0;
    mock_console_length = 0;
    mock_socket_result = 7; mock_connect_result = 0; mock_send_result_mode = 0;
    mock_close_result = 0; mock_recv_result = 0;
    mock_sent_length = 0; mock_sent_fd = mock_sent_flags = 0;
    mock_received_fd = mock_received_flags = 0;
    mock_dns_result = 0x0C7100CBu;
    mock_random_failure = 0;
    mock_rtc_failure = 0;
}

#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

static void init_user_regs(struct registers* regs, uint64_t number) {
    memset(regs, 0, sizeof(*regs));
    regs->cs = 0x23;
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
    init_user_regs(&regs, SYS_PRINT); regs.rdi = TEST_USER_BASE;
    syscall_handler(&regs);
    CHECK("SYS_PRINT reads its pointer from RDI", regs.rdi == TEST_USER_BASE);
    CHECK("SYS_PRINT returns payload length in RAX", regs.rax == 5);
    CHECK("SYS_PRINT logs the copied user string", strcmp(last_log, "hello") == 0);

    reset_mock();
    init_user_regs(&regs, SYS_PRINT); regs.rdi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE + 1u;
    syscall_handler(&regs);
    CHECK("SYS_PRINT rejects an invalid user pointer with -EFAULT", (int64_t)regs.rax == -14);

    reset_mock();
    init_user_regs(&regs, SYS_GETPID); regs.cs = 0x08;
    syscall_handler(&regs);
    CHECK("syscalls reject a supervisor-originated register frame", (int64_t)regs.rax == -22);

    reset_mock();
    mock_process.user_mode = 0;
    init_user_regs(&regs, SYS_GETPID);
    syscall_handler(&regs);
    CHECK("syscalls reject a Ring 3 frame without a user-mode process", (int64_t)regs.rax == -22);

    reset_mock();
    mock_process.page_directory = 0;
    init_user_regs(&regs, SYS_YIELD);
    syscall_handler(&regs);
    CHECK("SYS_YIELD rejects a process without a private address space",
          (int64_t)regs.rax == -22 && mock_yield_calls == 0);

    reset_mock();
    mock_no_current = 1;
    init_user_regs(&regs, SYS_YIELD);
    syscall_handler(&regs);
    CHECK("SYS_YIELD rejects a missing current process",
          (int64_t)regs.rax == -22 && mock_yield_calls == 0);

    reset_mock();
    init_user_regs(&regs, 0xFFFFu); syscall_handler(&regs);
    CHECK("unknown syscall numbers return -ENOSYS", (int64_t)regs.rax == -38);

    reset_mock();
    init_user_regs(&regs, SYS_YIELD); syscall_handler(&regs);
    CHECK("SYS_YIELD returns zero", regs.rax == 0);
    CHECK("SYS_YIELD reaches the scheduler", mock_yield_calls == 1);

    reset_mock();
    init_user_regs(&regs, SYS_SLEEP); regs.rdi = 4; syscall_handler(&regs);
    CHECK("SYS_SLEEP accepts its duration in RDI", mock_process.wake_tick == 104);
    CHECK("SYS_SLEEP transitions the process to SLEEPING", mock_process.state == PROCESS_STATE_SLEEPING);
    CHECK("SYS_SLEEP returns zero and yields", regs.rax == 0 && mock_yield_calls == 1);

    reset_mock();
    mock_uptime_ticks = 200; init_user_regs(&regs, SYS_SLEEP); regs.rdi = 0; syscall_handler(&regs);
    CHECK("zero-duration SYS_SLEEP yields for at least one tick",
          mock_process.wake_tick == 201 && mock_process.state == PROCESS_STATE_SLEEPING);

    reset_mock();
    mock_uptime_ticks = 0x7FFFFFFFFFFFFFFEULL; init_user_regs(&regs, SYS_SLEEP); regs.rdi = 2;
    syscall_handler(&regs);
    CHECK("SYS_SLEEP rejects tick overflow", (int64_t)regs.rax == -22);
    CHECK("overflowing SYS_SLEEP does not alter scheduler state",
          mock_process.state == PROCESS_STATE_RUNNING && mock_yield_calls == 0);

    reset_mock();
    init_user_regs(&regs, SYS_EXIT); regs.rdi = 0; syscall_handler(&regs);
    CHECK("SYS_EXIT returns status zero in its saved frame", regs.rax == 0);
    CHECK("SYS_EXIT destroys the calling PID", mock_destroy_calls == 1 && mock_destroyed_pid == 42);
    CHECK("SYS_EXIT marks termination and yields",
          mock_process.state == PROCESS_STATE_TERMINATED && mock_yield_calls == 1);

    /* The browser/TLS process must use copied user buffers, never kernel pointers. */
    reset_mock();
    init_user_regs(&regs, SYS_SOCKET); regs.rdi = AF_INET; regs.rsi = SOCK_STREAM; regs.rdx = 6u;
    syscall_handler(&regs);
    CHECK("SYS_SOCKET reaches the IPv4 stream socket API", (int64_t)regs.rax == 7 && mock_socket_calls == 1);

    reset_mock();
    sockaddr_in_t remote = {0};
    remote.sin_family = AF_INET; remote.sin_port = 0xBB01u; remote.sin_addr = 0x0C7100CBu;
    memcpy(mock_user_memory + 8u, &remote, sizeof(remote));
    init_user_regs(&regs, SYS_CONNECT); regs.rdi = 7; regs.rsi = TEST_USER_BASE + 8u;
    syscall_handler(&regs);
    CHECK("SYS_CONNECT copies the user socket address", regs.rax == 0 &&
          mock_connect_calls == 1 && mock_connected_address.sin_port == remote.sin_port &&
          mock_connected_address.sin_addr == remote.sin_addr);
    init_user_regs(&regs, SYS_CONNECT); regs.rdi = 7; regs.rsi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE - 4u;
    syscall_handler(&regs);
    CHECK("SYS_CONNECT rejects a partially unmapped address", (int64_t)regs.rax == -14);

    reset_mock();
    memcpy(mock_user_memory + 32u, "ClientHello", 11u);
    init_user_regs(&regs, SYS_SEND); regs.rdi = 7; regs.rsi = TEST_USER_BASE + 32u;
    regs.rdx = 11u; regs.rcx = 0u; syscall_handler(&regs);
    CHECK("SYS_SEND copies payload into the kernel before transport", regs.rax == 11u &&
          mock_send_calls == 1 && mock_sent_fd == 7 && mock_sent_length == 11u &&
          memcmp(mock_sent_bytes, "ClientHello", 11u) == 0);
    init_user_regs(&regs, SYS_SEND); regs.rdi = 7; regs.rsi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE;
    regs.rdx = 1u; syscall_handler(&regs);
    CHECK("SYS_SEND rejects inaccessible user buffers", (int64_t)regs.rax == -14);
    init_user_regs(&regs, SYS_SEND); regs.rdi = 7; regs.rsi = TEST_USER_BASE;
    regs.rdx = TCP_MSS + 1u; syscall_handler(&regs);
    CHECK("SYS_SEND rejects oversized single-segment writes", (int64_t)regs.rax == -22);

    reset_mock();
    init_user_regs(&regs, SYS_RECV); regs.rdi = 7; regs.rsi = TEST_USER_BASE + 48u;
    regs.rdx = 64u; syscall_handler(&regs);
    CHECK("SYS_RECV copies received data to the calling user address space",
          regs.rax == 9u && memcmp(mock_user_memory + 48u, "TLS reply", 9u) == 0);
    init_user_regs(&regs, SYS_RECV); regs.rdi = 7; regs.rsi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE - 2u;
    regs.rdx = 16u; syscall_handler(&regs);
    CHECK("SYS_RECV rejects a buffer beyond the user mapping", (int64_t)regs.rax == -14);

    reset_mock();
    memcpy(mock_user_memory + 64u, "example.com", sizeof("example.com"));
    init_user_regs(&regs, SYS_DNS_RESOLVE); regs.rdi = TEST_USER_BASE + 64u; syscall_handler(&regs);
    CHECK("SYS_DNS_RESOLVE copies and resolves a hostname", regs.rax == mock_dns_result &&
          mock_dns_calls == 1 && strcmp(mock_dns_name, "example.com") == 0);
    init_user_regs(&regs, SYS_DNS_RESOLVE); regs.rdi = TEST_USER_BASE + TEST_USER_MEMORY_SIZE + 1u;
    syscall_handler(&regs);
    CHECK("SYS_DNS_RESOLVE rejects inaccessible hostname pointers", (int64_t)regs.rax == -14);

    reset_mock();
    memcpy(mock_user_memory + 96u, "hello", 5u);
    init_user_regs(&regs, SYS_WRITE); regs.rdi = 1u; regs.rsi = TEST_USER_BASE + 96u; regs.rdx = 5u;
    syscall_handler(&regs);
    CHECK("SYS_WRITE accepts a length-delimited user buffer", regs.rax == 5u && strcmp(mock_console, "hello") == 0);
    init_user_regs(&regs, SYS_WRITE); regs.rdi = 4u; regs.rsi = TEST_USER_BASE + 96u; regs.rdx = 5u;
    syscall_handler(&regs);
    CHECK("SYS_WRITE rejects unsupported file descriptors", (int64_t)regs.rax == -9);

    reset_mock();
    init_user_regs(&regs, SYS_CLOSE); regs.rdi = 7u; syscall_handler(&regs);
    CHECK("SYS_CLOSE delegates descriptor cleanup", regs.rax == 0u && mock_close_calls == 1);


    reset_mock();
    init_user_regs(&regs,SYS_RANDOM);regs.rdi=TEST_USER_BASE+16u;regs.rsi=8u;syscall_handler(&regs);
    CHECK("SYS_RANDOM copies entropy into user memory",regs.rax==8u&&mock_user_memory[16]==0xA0u&&mock_user_memory[23]==0xA7u);
    init_user_regs(&regs,SYS_RANDOM);regs.rdi=TEST_USER_BASE+TEST_USER_MEMORY_SIZE+1u;regs.rsi=8u;syscall_handler(&regs);
    CHECK("SYS_RANDOM rejects inaccessible user memory",(int64_t)regs.rax==-14);
    init_user_regs(&regs,SYS_RANDOM);regs.rdi=TEST_USER_BASE+16u;regs.rsi=257u;syscall_handler(&regs);
    CHECK("SYS_RANDOM rejects oversized requests",(int64_t)regs.rax==-22);
    mock_random_failure=1;
    init_user_regs(&regs,SYS_RANDOM);regs.rdi=TEST_USER_BASE+16u;regs.rsi=8u;syscall_handler(&regs);
    CHECK("SYS_RANDOM fails closed when entropy is unavailable",(int64_t)regs.rax==-38);
    reset_mock();
    init_user_regs(&regs,SYS_TIME);syscall_handler(&regs);
    CHECK("SYS_TIME returns RTC Unix seconds",regs.rax==1700000000ULL);
    mock_rtc_failure=1;init_user_regs(&regs,SYS_TIME);syscall_handler(&regs);
    CHECK("SYS_TIME fails closed when RTC is invalid",(int64_t)regs.rax==-5);
    puts("Syscall ABI tests passed.");
    return 0;
}

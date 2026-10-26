#include "elf.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"
#include "../syscall/syscall.h"
#include "../memory/vmm.h"
#include "../memory/pmm.h"
#include "../log/logger.h"
#include <stdint.h>
#include <stddef.h>

#define SMOKE_BASE 0x0000000100000000ULL
#define SMOKE_FILE_OFFSET 0x1000u
#define SMOKE_IMAGE_SIZE 0x1300u
#define SMOKE_CODE_CAPACITY 256u
#define SMOKE_BAD_USER_POINTER 0x00007FFE00000000ULL
#define SMOKE_TEST_TICK_LIMIT 3000ULL

static uint8_t smoke_image_a[SMOKE_IMAGE_SIZE];
static uint8_t smoke_image_b[SMOKE_IMAGE_SIZE];
static uint8_t malformed_image[SMOKE_IMAGE_SIZE];
static uint32_t smoke_pid_a;
static uint32_t smoke_pid_b;
static uint32_t smoke_baseline_process_count;

typedef struct smoke_code_builder {
    uint8_t bytes[SMOKE_CODE_CAPACITY];
    size_t length;
    size_t fail_branch_offsets[8];
    size_t fail_branch_count;
    size_t success_message_operand;
    size_t failure_message_operand;
    int failed;
} smoke_code_builder_t;

static size_t smoke_string_length(const char* text) {
    size_t length = 0;
    if (text) while (text[length]) ++length;
    return length;
}

static void smoke_emit_u8(smoke_code_builder_t* code, uint8_t value) {
    if (!code || code->length >= sizeof(code->bytes)) {
        if (code) code->failed = 1;
        return;
    }
    code->bytes[code->length++] = value;
}

static void smoke_emit_u32(smoke_code_builder_t* code, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        smoke_emit_u8(code, (uint8_t)(value >> (i * 8u)));
}

static void smoke_emit_u64(smoke_code_builder_t* code, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        smoke_emit_u8(code, (uint8_t)(value >> (i * 8u)));
}

static void smoke_emit_syscall_number(smoke_code_builder_t* code, uint32_t number) {
    smoke_emit_u8(code, 0xB8); /* mov eax, imm32 */
    smoke_emit_u32(code, number);
}

static void smoke_emit_int80(smoke_code_builder_t* code) {
    smoke_emit_u8(code, 0xCD);
    smoke_emit_u8(code, 0x80);
}

static void smoke_emit_test_rax(smoke_code_builder_t* code) {
    smoke_emit_u8(code, 0x48);
    smoke_emit_u8(code, 0x85);
    smoke_emit_u8(code, 0xC0); /* test rax, rax */
}

static void smoke_emit_fail_branch(smoke_code_builder_t* code, uint8_t condition) {
    if (code->fail_branch_count >=
        sizeof(code->fail_branch_offsets) / sizeof(code->fail_branch_offsets[0])) {
        code->failed = 1;
        return;
    }
    smoke_emit_u8(code, 0x0F);
    smoke_emit_u8(code, condition); /* 0x84 = JE, 0x85 = JNE */
    code->fail_branch_offsets[code->fail_branch_count++] = code->length;
    smoke_emit_u32(code, 0);
}

static size_t smoke_emit_movabs_rdi(smoke_code_builder_t* code, uint64_t value) {
    smoke_emit_u8(code, 0x48);
    smoke_emit_u8(code, 0xBF); /* mov rdi, imm64 */
    size_t operand = code->length;
    smoke_emit_u64(code, value);
    return operand;
}

static void smoke_patch_u64(uint8_t* target, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        target[i] = (uint8_t)(value >> (i * 8u));
}

static void smoke_patch_u32(uint8_t* target, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        target[i] = (uint8_t)(value >> (i * 8u));
}

/*
 * Both programs use the same virtual addresses but run in distinct address
 * spaces and contain different markers. They check syscall arguments and
 * return values before requesting process termination.
 */
static int smoke_build_user_image(uint8_t* image, const char* success_message,
                                  uint64_t* image_size_out,
                                  uint64_t* message_address_out,
                                  size_t* message_length_out) {
    static const char failure_message[] = "PHOXTRA_USERMODE_TEST_FAILED";
    if (!image || !success_message || !image_size_out ||
        !message_address_out || !message_length_out) return -1;

    smoke_code_builder_t code = {0};
    size_t message_length = smoke_string_length(success_message);
    size_t failure_length = sizeof(failure_message) - 1u;

    /* SYS_GETPID: number in RAX, PID returned in RAX. */
    smoke_emit_syscall_number(&code, SYS_GETPID);
    smoke_emit_int80(&code);
    smoke_emit_test_rax(&code);
    smoke_emit_fail_branch(&code, 0x84); /* fail if PID == 0 */

    /* SYS_YIELD returns zero. */
    smoke_emit_syscall_number(&code, SYS_YIELD);
    smoke_emit_int80(&code);
    smoke_emit_test_rax(&code);
    smoke_emit_fail_branch(&code, 0x85); /* fail if return != 0 */

    /* SYS_SLEEP receives ticks in RDI and returns zero after waking. */
    smoke_emit_syscall_number(&code, SYS_SLEEP);
    smoke_emit_u8(&code, 0x48);
    smoke_emit_u8(&code, 0xC7);
    smoke_emit_u8(&code, 0xC7);
    smoke_emit_u32(&code, 1); /* mov rdi, 1 tick */
    smoke_emit_int80(&code);
    smoke_emit_test_rax(&code);
    smoke_emit_fail_branch(&code, 0x85);

    /* SYS_PRINT takes a user pointer in RDI and returns its length in RAX. */
    smoke_emit_syscall_number(&code, SYS_PRINT);
    code.success_message_operand = smoke_emit_movabs_rdi(&code, 0);
    smoke_emit_int80(&code);
    smoke_emit_u8(&code, 0x48);
    smoke_emit_u8(&code, 0xBB); /* mov rbx, expected length */
    smoke_emit_u64(&code, (uint64_t)message_length);
    smoke_emit_u8(&code, 0x48);
    smoke_emit_u8(&code, 0x39);
    smoke_emit_u8(&code, 0xD8); /* cmp rax, rbx */
    smoke_emit_fail_branch(&code, 0x85);

    /* An unmapped canonical user pointer must return -EFAULT without crashing. */
    smoke_emit_syscall_number(&code, SYS_PRINT);
    (void)smoke_emit_movabs_rdi(&code, SMOKE_BAD_USER_POINTER);
    smoke_emit_int80(&code);
    smoke_emit_u8(&code, 0x48);
    smoke_emit_u8(&code, 0xBB); /* mov rbx, -14 */
    smoke_emit_u64(&code, (uint64_t)(int64_t)-14);
    smoke_emit_u8(&code, 0x48);
    smoke_emit_u8(&code, 0x39);
    smoke_emit_u8(&code, 0xD8); /* cmp rax, rbx */
    smoke_emit_fail_branch(&code, 0x85);

    /* Successful process exit. */
    smoke_emit_syscall_number(&code, SYS_EXIT);
    smoke_emit_u8(&code, 0x31);
    smoke_emit_u8(&code, 0xFF); /* xor edi, edi: status 0 */
    smoke_emit_int80(&code);
    smoke_emit_u8(&code, 0xEB);
    smoke_emit_u8(&code, 0xFE); /* should not return */

    size_t failure_offset = code.length;
    smoke_emit_syscall_number(&code, SYS_PRINT);
    code.failure_message_operand = smoke_emit_movabs_rdi(&code, 0);
    smoke_emit_int80(&code);
    smoke_emit_syscall_number(&code, SYS_EXIT);
    smoke_emit_u8(&code, 0xBF); /* mov edi, 1: failure status */
    smoke_emit_u32(&code, 1);
    smoke_emit_int80(&code);
    smoke_emit_u8(&code, 0xEB);
    smoke_emit_u8(&code, 0xFE);

    if (code.failed || code.length > SMOKE_CODE_CAPACITY) return -1;

    for (size_t i = 0; i < code.fail_branch_count; ++i) {
        size_t operand = code.fail_branch_offsets[i];
        int64_t displacement = (int64_t)failure_offset - (int64_t)(operand + 4u);
        if (displacement < INT32_MIN || displacement > INT32_MAX) return -1;
        smoke_patch_u32(&code.bytes[operand], (uint32_t)(int32_t)displacement);
    }

    size_t payload_size = code.length + message_length + 1u + failure_length + 1u;
    if (SMOKE_FILE_OFFSET + payload_size > SMOKE_IMAGE_SIZE ||
        code.success_message_operand + 8u > code.length ||
        code.failure_message_operand + 8u > code.length) return -1;

    uint64_t message_address = SMOKE_BASE + code.length;
    uint64_t failure_address = message_address + message_length + 1u;
    smoke_patch_u64(&code.bytes[code.success_message_operand], message_address);
    smoke_patch_u64(&code.bytes[code.failure_message_operand], failure_address);

    for (size_t i = 0; i < SMOKE_IMAGE_SIZE; ++i) image[i] = 0;
    elf64_header_t* h = (elf64_header_t*)image;
    h->e_ident[0] = 0x7F; h->e_ident[1] = 'E'; h->e_ident[2] = 'L'; h->e_ident[3] = 'F';
    h->e_ident[4] = ELF_CLASS_64; h->e_ident[5] = ELF_DATA_LSB; h->e_ident[6] = ELF_VERSION_CURRENT;
    h->e_type = ELF_TYPE_EXEC; h->e_machine = ELF_ARCH_X86_64; h->e_version = ELF_VERSION_CURRENT;
    h->e_entry = SMOKE_BASE; h->e_phoff = sizeof(*h); h->e_ehsize = sizeof(*h);
    h->e_phentsize = sizeof(elf64_phdr_t); h->e_phnum = 1;

    elf64_phdr_t* p = (elf64_phdr_t*)(image + h->e_phoff);
    p->p_type = PT_LOAD;
    p->p_flags = PF_R | PF_X;
    p->p_offset = SMOKE_FILE_OFFSET;
    p->p_vaddr = SMOKE_BASE;
    p->p_filesz = payload_size;
    p->p_memsz = payload_size;
    p->p_align = PAGE_SIZE;

    for (size_t i = 0; i < code.length; ++i)
        image[SMOKE_FILE_OFFSET + i] = code.bytes[i];
    for (size_t i = 0; i < message_length; ++i)
        image[SMOKE_FILE_OFFSET + code.length + i] = (uint8_t)success_message[i];
    image[SMOKE_FILE_OFFSET + code.length + message_length] = 0;
    for (size_t i = 0; i < failure_length; ++i)
        image[SMOKE_FILE_OFFSET + code.length + message_length + 1u + i] =
            (uint8_t)failure_message[i];
    image[SMOKE_FILE_OFFSET + code.length + message_length + 1u + failure_length] = 0;

    *image_size_out = SMOKE_FILE_OFFSET + payload_size;
    *message_address_out = message_address;
    *message_length_out = message_length;
    return 0;
}

static int smoke_verify_private_images(pcb_t* process_a, pcb_t* process_b,
                                       uint64_t message_address, size_t message_length) {
    if (!process_a || !process_b || process_a == process_b ||
        process_a->pid == process_b->pid ||
        !process_a->page_directory || !process_b->page_directory ||
        process_a->page_directory == process_b->page_directory ||
        !message_length) return 0;

    uint8_t marker_a = 0, marker_b = 0;
    /* The distinguishing A/B byte is at offset 17 in each message. */
    if (vmm_copy_from_user(process_a->page_directory, &marker_a,
                           message_address + 17u, 1) != 0 ||
        vmm_copy_from_user(process_b->page_directory, &marker_b,
                           message_address + 17u, 1) != 0)
        return 0;
    return marker_a == 'A' && marker_b == 'B' && marker_a != marker_b;
}

static void smoke_process_monitor(void) {
    klog(LOG_INFO, "PHOXTRA_PROCESS_MONITOR_ENTERED");
    uint64_t start_tick = scheduler_get_uptime_ticks();
    int timer_wake_logged = 0;
    while (process_lookup(smoke_pid_a) || process_lookup(smoke_pid_b)) {
        if (scheduler_get_uptime_ticks() - start_tick > SMOKE_TEST_TICK_LIMIT) {
            klog(LOG_ERROR, "PHOXTRA_PROCESS_CLEANUP_FAILED: timeout");
            return;
        }
        __asm__ volatile("sti; hlt" ::: "memory");
        if (!timer_wake_logged) {
            klog(LOG_INFO, "PHOXTRA_PROCESS_MONITOR_TIMER_WAKE");
            timer_wake_logged = 1;
        }
    }

    if (process_count() != smoke_baseline_process_count + 1u) {
        klog(LOG_ERROR, "PHOXTRA_PROCESS_CLEANUP_FAILED: process table count mismatch");
        return;
    }
    klog(LOG_INFO, "PHOXTRA_PROCESS_CLEANUP_OK");
}

void user_mode_smoke_init(void) {
    static const char message_a[] = "PHOXTRA_USERMODE_A_OK";
    static const char message_b[] = "PHOXTRA_USERMODE_B_OK";
    smoke_baseline_process_count = process_count();

    uint64_t image_size_a = 0, image_size_b = 0;
    uint64_t message_address_a = 0, message_address_b = 0;
    size_t message_length_a = 0, message_length_b = 0;
    if (smoke_build_user_image(smoke_image_a, message_a, &image_size_a,
                               &message_address_a, &message_length_a) != 0 ||
        smoke_build_user_image(smoke_image_b, message_b, &image_size_b,
                               &message_address_b, &message_length_b) != 0) {
        klog(LOG_ERROR, "PHOXTRA_USERMODE_TEST_SETUP_FAILED: image generation");
        return;
    }
    if (image_size_a != image_size_b ||
        message_address_a != message_address_b ||
        message_length_a != message_length_b) {
        klog(LOG_ERROR, "PHOXTRA_USERMODE_TEST_SETUP_FAILED: image layout mismatch");
        return;
    }

    /* Malformed program headers must be rejected without registering a process. */
    for (size_t i = 0; i < sizeof(malformed_image); ++i) malformed_image[i] = smoke_image_a[i];
    ((elf64_header_t*)malformed_image)->e_phoff = UINT64_MAX - 4u;
    uint64_t ignored_entry = 0;
    uint32_t count_before_bad_elf = process_count();
    pcb_t* malformed = process_create_user_from_elf("malformed-elf", malformed_image,
                                                      image_size_a, 1);
    if (malformed || elf_validate_image(malformed_image, image_size_a, &ignored_entry) ||
        process_count() != count_before_bad_elf) {
        klog(LOG_ERROR, "PHOXTRA_MALFORMED_ELF_REJECTION_FAILED");
        return;
    }
    klog(LOG_INFO, "PHOXTRA_MALFORMED_ELF_REJECTION_OK");

    pcb_t* process_a = process_create_user_from_elf("usermode-test-a", smoke_image_a,
                                                     image_size_a, 1);
    pcb_t* process_b = process_create_user_from_elf("usermode-test-b", smoke_image_b,
                                                     image_size_b, 1);
    if (!process_a || !process_b) {
        klog(LOG_ERROR, "PHOXTRA_USERMODE_TEST_SETUP_FAILED: process creation");
        return;
    }
    smoke_pid_a = process_a->pid;
    smoke_pid_b = process_b->pid;

    if (!smoke_verify_private_images(process_a, process_b, message_address_a,
                                     message_length_a)) {
        klog(LOG_ERROR, "PHOXTRA_PROCESS_ISOLATION_FAILED");
        return;
    }
    klog(LOG_INFO, "PHOXTRA_PROCESS_ISOLATION_OK");

    if (!process_create("usermode-test-monitor", smoke_process_monitor, 1)) {
        klog(LOG_ERROR, "PHOXTRA_USERMODE_TEST_SETUP_FAILED: monitor creation");
        return;
    }
    klog(LOG_KERNEL, "PHOXTRA_USERMODE_TESTS_QUEUED");
}

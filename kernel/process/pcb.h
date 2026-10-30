#ifndef PHOXTRA_PCB_H
#define PHOXTRA_PCB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_WAITING,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct cpu_registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_registers_t;

typedef struct pcb {
    uint32_t pid;
    uint32_t parent_pid;
    char name[32];
    process_state_t state;
    uint32_t priority;
    uint64_t rsp;
    uint64_t rip;
    cpu_registers_t regs;
    uint64_t page_directory;
    uint64_t user_entry;
    uint64_t user_stack;
    uint32_t user_mode;
    uint32_t security_context;
    uint64_t creation_time;
    uint64_t wake_tick;
    uint8_t  process_hash[32];
    uint8_t  signature[64];
    uint32_t identity_status;
    uint64_t capabilities;
    uint32_t mac_label;
    void* stack_base;
    uint64_t stack_size;
    struct pcb* next;
    /* Architectural x87/MMX/SSE register image saved with FXSAVE64. */
    uint8_t fpu_state[512] __attribute__((aligned(16)));
} pcb_t;

void pcb_process_entry_trampoline(void);
void pcb_user_entry_trampoline(void);
pcb_t* pcb_create(uint32_t pid, const char* name, void (*entry)(void), uint32_t priority, uint32_t parent_pid);
int pcb_destroy(pcb_t* pcb);
const char* pcb_state_to_string(process_state_t state);

#ifdef __cplusplus
}
#endif

#endif

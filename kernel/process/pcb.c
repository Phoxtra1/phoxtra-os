#include "pcb.h"
#include "../memory/heap.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../scheduler/scheduler.h"
#include "../log/logger.h"
#include "../security/process_identity.h"
#include "../security/audit_logger.h"
#include "../security/capability.h"
#include "../security/mac.h"

#define KERNEL_STACK_SIZE (16 * 1024) // 16KB stack per process
extern void switch_to_user_mode(uint64_t entry_point, uint64_t user_rsp);
extern void gdt_set_kernel_stack(uint64_t rsp0);

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

const char* pcb_state_to_string(process_state_t state) {
    switch (state) {
        case PROCESS_STATE_NEW:        return "NEW";
        case PROCESS_STATE_READY:      return "READY";
        case PROCESS_STATE_RUNNING:    return "RUNNING";
        case PROCESS_STATE_WAITING:    return "WAITING";
        case PROCESS_STATE_SLEEPING:   return "SLEEPING";
        case PROCESS_STATE_TERMINATED: return "TERMINATED";
        default:                       return "UNKNOWN";
    }
}

pcb_t* pcb_create(uint32_t pid, const char* name, void (*entry)(void), uint32_t priority, uint32_t parent_pid) {
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    if (!pcb) {
        klog(LOG_ERROR, "PCB: Failed to allocate process control block");
        return (void*)0;
    }

    // Zero out structure
    for (size_t i = 0; i < sizeof(pcb_t); i++)
        ((uint8_t*)pcb)[i] = 0;

    // Allocate kernel stack
    void* stack = kmalloc(KERNEL_STACK_SIZE);
    if (!stack) {
        klog(LOG_ERROR, "PCB: Failed to allocate kernel stack");
        kfree(pcb);
        return (void*)0;
    }

    pcb->pid            = pid;
    pcb->parent_pid     = parent_pid;
    pcb->priority       = priority;
    pcb->state          = PROCESS_STATE_NEW;
    pcb->stack_base     = stack;
    pcb->stack_size     = KERNEL_STACK_SIZE;
    pcb->page_directory = vmm_create_user_page_directory();
    if (!pcb->page_directory) {
        klog(LOG_ERROR, "PCB: Failed to create private process address space");
        kfree(stack);
        kfree(pcb);
        return (void*)0;
    }
    pcb->next           = (void*)0;

    kstrncpy(pcb->name, name, 32);

    pcb->rip = (uint64_t)entry;


    // Set up the initial context_switch frame. It restores six callee-saved
    // registers, returns to pcb_process_entry_trampoline, and leaves a synthetic
    // return address so the C entry obeys the SysV AMD64 stack alignment.
    uint64_t stack_top = ((uint64_t)stack + KERNEL_STACK_SIZE) & ~0xFULL;
    uint64_t* sp = (uint64_t*)stack_top;

    *--sp = 0; // synthetic return address; keeps SysV entry alignment
    *--sp = (uint64_t)pcb_process_entry_trampoline; // ret target
    *--sp = 0;               // r15
    *--sp = 0;               // r14
    *--sp = 0;               // r13
    *--sp = 0;               // r12
    *--sp = 0;               // rbp
    *--sp = 0;               // rbx  ← RSP

    pcb->rsp = (uint64_t)sp;

    // Assign CAC Capabilities & MAC Security Domain Label
    if (pid == 1 || name[0] == 'k') { // Kernel system process
        pcb->capabilities = CAP_ALL;
        pcb->mac_label    = MAC_LABEL_SYSTEM;
    } else if (name[0] == 's') { // Security / Shell process
        pcb->capabilities = CAP_SYS_ADMIN | CAP_DEV_ACCESS | CAP_IPC_SEND | CAP_SECURITY_AUDIT;
        pcb->mac_label    = MAC_LABEL_SERVICE;
    } else {
        pcb->capabilities = CAP_IPC_SEND | CAP_DEV_ACCESS;
        pcb->mac_label    = MAC_LABEL_USER;
    }

    // Cryptographically bind process identity & SHA-256 process hash
    process_identity_bind(pcb, (const uint8_t*)entry, 128);

    char audit_msg[64];
    audit_msg[0] = 'P'; audit_msg[1] = 'r'; audit_msg[2] = 'o'; audit_msg[3] = 'c'; audit_msg[4] = 'e'; audit_msg[5] = 's'; audit_msg[6] = 's'; audit_msg[7] = ' '; audit_msg[8] = 'c'; audit_msg[9] = 'r'; audit_msg[10] = 'e'; audit_msg[11] = 'a'; audit_msg[12] = 't'; audit_msg[13] = 'e'; audit_msg[14] = 'd'; audit_msg[15] = '\0';
    audit_log_event(AUDIT_TYPE_PROCESS_BINDING, pcb->pid, audit_msg);

    return pcb;
}

int pcb_destroy(pcb_t* pcb) {
    if (!pcb) return -1;
    /*
     * Address-space teardown refuses the active CR3. Never free a PCB or its
     * stack if its page tables could still be in use; a scheduler-side reaper
     * can reclaim such a process after switching away from it.
     */
    if (pcb->page_directory &&
        vmm_destroy_user_page_directory(pcb->page_directory) != 0) {
        return -1;
    }
    pcb->page_directory = 0;
    if (pcb->stack_base) kfree(pcb->stack_base);
    kfree(pcb);
    return 0;
}

/* Start every fresh task with interrupts enabled and reap it if its entry returns. */
void pcb_process_entry_trampoline(void) {
    klog(LOG_INFO, "Kernel process entry trampoline entered");
    pcb_t* current = scheduler_current();
    if (!current || !current->rip) {
        for (;;) __asm__ volatile("cli; hlt");
    }
    __asm__ volatile("sti" ::: "memory");
    ((void (*)(void))(uintptr_t)current->rip)();
    current->state = PROCESS_STATE_TERMINATED;
    scheduler_yield();
    for (;;) __asm__ volatile("cli; hlt");
}

/* Initial kernel context for an ELF process; iretq enters Ring 3 and never returns. */
void pcb_user_entry_trampoline(void) {
    klog(LOG_INFO, "Ring 3 process trampoline entered");
    pcb_t* current = scheduler_current();
    if (!current || !current->user_mode || !current->user_entry || !current->user_stack) {
        if (current) current->state = PROCESS_STATE_TERMINATED;
        scheduler_yield();
        for (;;) __asm__ volatile("cli; hlt");
    }
    gdt_set_kernel_stack((uint64_t)(uintptr_t)current->stack_base + current->stack_size);
    klog(LOG_INFO, "Entering Ring 3 via IRETQ");
    switch_to_user_mode(current->user_entry, current->user_stack);
    current->state = PROCESS_STATE_TERMINATED;
    scheduler_yield();
    for (;;) __asm__ volatile("cli; hlt");
}

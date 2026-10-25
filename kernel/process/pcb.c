#include "pcb.h"
#include "../memory/heap.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../log/logger.h"
#include "../security/process_identity.h"
#include "../security/audit_logger.h"
#include "../security/capability.h"
#include "../security/mac.h"

#define KERNEL_STACK_SIZE (16 * 1024) // 16KB stack per process

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


    // Set up initial stack frame for context_switch:
    // context_switch() does: pop rbx, pop rbp, pop r12, pop r13, pop r14, pop r15, ret
    // Stack layout (growing downward, RSP points to lowest slot = rbx):
    //   [RSP+48] = entry   (ret target — jumped to after the 6 pops)
    //   [RSP+40] = r15
    //   [RSP+32] = r14
    //   [RSP+24] = r13
    //   [RSP+16] = r12
    //   [RSP+8]  = rbp
    //   [RSP+0]  = rbx  ← RSP points here
    uint64_t stack_top = ((uint64_t)stack + KERNEL_STACK_SIZE) & ~0xFULL;
    uint64_t* sp = (uint64_t*)stack_top;

    *--sp = (uint64_t)entry; // ret target
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

void pcb_destroy(pcb_t* pcb) {
    if (!pcb) return;
    /*
     * Address-space teardown refuses the active CR3. Never free a PCB or its
     * stack if its page tables could still be in use; a scheduler-side reaper
     * can reclaim such a process after switching away from it.
     */
    if (pcb->page_directory &&
        vmm_destroy_user_page_directory(pcb->page_directory) != 0) {
        klog(LOG_WARNING, "PCB: address space is active or not owned; retaining process resources");
        return;
    }
    pcb->page_directory = 0;
    if (pcb->stack_base) kfree(pcb->stack_base);
    kfree(pcb);
}

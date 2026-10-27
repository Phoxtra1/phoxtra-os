#include "process.h"
#include "../exec/elf.h"
#include "pcb.h"
#include "../log/logger.h"
#include "../memory/heap.h"
#include "../scheduler/scheduler.h"

static pcb_t* process_table_arr[MAX_PROCESSES];
static uint32_t next_pid = 0;
static uint32_t active_count = 0;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        process_table_arr[i] = (void*)0;
    next_pid = 0;
    active_count = 0;
    klog(LOG_KERNEL, "Process Manager initialized");
}

pcb_t* process_create(const char* name, void (*entry)(void), uint32_t priority) {
    if (!name || !name[0] || !entry) {
        klog(LOG_ERROR, "Process Manager: rejected invalid process name or entry point");
        return (void*)0;
    }
    if (active_count >= MAX_PROCESSES) {
        klog(LOG_ERROR, "Process Manager: process table full");
        return (void*)0;
    }

    uint32_t pid = next_pid++;
    pcb_t* pcb = pcb_create(pid, name, entry, priority, 0);
    if (!pcb) return (void*)0;

    // Find free slot
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_table_arr[i]) {
            process_table_arr[i] = pcb;
            active_count++;
            pcb->state = PROCESS_STATE_READY;
            scheduler_add_process(pcb);
            return pcb;
        }
    }

    pcb_destroy(pcb);
    return (void*)0;
}

pcb_t* process_create_user_from_elf(const char* name, const uint8_t* image,
                                    uint64_t image_size, uint32_t priority) {
    if (!name || !name[0] || !image || !image_size || active_count >= MAX_PROCESSES) return (void*)0;
    pcb_t* pcb = pcb_create(next_pid++, name, pcb_user_entry_trampoline, priority, 0);
    if (!pcb) return (void*)0;

    uint64_t entry = 0, stack_top = 0;
    if (elf_load_into_address_space(pcb->page_directory, image, image_size,
                                    &entry, &stack_top) != 0) {
        (void)pcb_destroy(pcb);
        klog(LOG_WARNING, "Process Manager: ELF load failed; process creation rolled back");
        return (void*)0;
    }

    pcb->user_mode = 1;
    pcb->user_entry = entry;
    pcb->user_stack = stack_top;
    pcb->rip = (uint64_t)pcb_user_entry_trampoline;

    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (!process_table_arr[i]) {
            process_table_arr[i] = pcb;
            ++active_count;
            pcb->state = PROCESS_STATE_READY;
            scheduler_add_process(pcb);
            return pcb;
        }
    }

    (void)pcb_destroy(pcb);
    return (void*)0;
}

void process_destroy(uint32_t pid) {
    scheduler_remove_process(pid);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table_arr[i] && process_table_arr[i]->pid == pid) {
            process_table_arr[i]->state = PROCESS_STATE_TERMINATED;
            if (pcb_destroy(process_table_arr[i]) == 0) {
                process_table_arr[i] = (void*)0;
                active_count--;
            }
            /* Active address spaces remain registered for the scheduler reaper. */
            return;
        }
    }
}

void process_reap_terminated(void) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        pcb_t* pcb = process_table_arr[i];
        if (!pcb || pcb->state != PROCESS_STATE_TERMINATED) continue;
        if (pcb_destroy(pcb) == 0) {
            process_table_arr[i] = (void*)0;
            if (active_count) --active_count;
        }
    }
}

void process_pause(uint32_t pid) {
    pcb_t* p = process_lookup(pid);
    if (p && p->state == PROCESS_STATE_RUNNING)
        p->state = PROCESS_STATE_WAITING;
}

void process_resume(uint32_t pid) {
    pcb_t* p = process_lookup(pid);
    if (p && p->state == PROCESS_STATE_WAITING) {
        p->state = PROCESS_STATE_READY;
        scheduler_add_process(p);
    }
}

pcb_t* process_lookup(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table_arr[i] && process_table_arr[i]->pid == pid)
            return process_table_arr[i];
    }
    return (void*)0;
}

uint32_t process_count(void) {
    return active_count;
}

pcb_t** process_table(void) {
    return process_table_arr;
}

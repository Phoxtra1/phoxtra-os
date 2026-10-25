#include "scheduler.h"
#include "../process/pcb.h"
#include "../process/process.h"
#include "../memory/vmm.h"
#include "../log/logger.h"
extern void gdt_set_kernel_stack(uint64_t rsp0);


#define MAX_READY_QUEUE 64
#define TIME_SLICE_TICKS 10   /* Switch every 10 timer ticks (100ms at 100Hz) */

/* Idle task declaration — defined further below */
static void idle_task(void);

/* Forward declaration of context_switch — implemented in context.asm */
extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);

/* ────────────────────────────────────────────────
   Internal Scheduler State
   ──────────────────────────────────────────────── */
static pcb_t*           ready_queue[MAX_READY_QUEUE];
static uint32_t         queue_head    = 0;
static uint32_t         queue_tail    = 0;
static uint32_t         queue_count   = 0;

static pcb_t*           current_process = (void*)0;
static pcb_t            idle_pcb;
static uint8_t          idle_stack[4096];

static sched_algorithm_t active_algorithm = SCHED_ROUND_ROBIN;
static uint64_t          tick_counter     = 0;
static uint64_t          current_ticks    = 0;

/* ────────────────────────────────────────────────
   Internal Queue Helpers
   ──────────────────────────────────────────────── */
static void queue_push(pcb_t* pcb) {
    if (queue_count >= MAX_READY_QUEUE) return;
    ready_queue[queue_tail] = pcb;
    queue_tail = (queue_tail + 1) % MAX_READY_QUEUE;
    queue_count++;
}

static pcb_t* queue_pop(void) {
    if (queue_count == 0) return (void*)0;
    pcb_t* pcb = ready_queue[queue_head];
    queue_head = (queue_head + 1) % MAX_READY_QUEUE;
    queue_count--;
    return pcb;
}

/* ────────────────────────────────────────────────
   Idle Process — runs only when nothing else can
   ──────────────────────────────────────────────── */
static void idle_task(void) {
    while (1) {
        asm volatile("hlt");
    }
}

static void idle_pcb_init(void) {
    for (size_t i = 0; i < sizeof(pcb_t); i++) ((uint8_t*)&idle_pcb)[i] = 0;

    uint64_t stack_top = ((uint64_t)idle_stack + sizeof(idle_stack)) & ~0xFULL;
    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = (uint64_t)idle_task; /* ret target after callee-saved restores */
    *--sp = 0; /* r15 */
    *--sp = 0; /* r14 */
    *--sp = 0; /* r13 */
    *--sp = 0; /* r12 */
    *--sp = 0; /* rbp */
    *--sp = 0; /* rbx */

    /* Build PCB by hand (no heap needed for idle) */
    static const char idle_name[] = "idle";
    for (int i = 0; i < 5; i++) idle_pcb.name[i] = idle_name[i];
    idle_pcb.pid          = 0xFFFFFFFF;
    idle_pcb.state        = PROCESS_STATE_READY;
    idle_pcb.priority     = 0;
    idle_pcb.rsp          = (uint64_t)sp;
    idle_pcb.rip          = (uint64_t)idle_task;
    idle_pcb.stack_base   = idle_stack;
    idle_pcb.stack_size   = sizeof(idle_stack);
    idle_pcb.regs.rflags  = 0x202;
    idle_pcb.regs.cs      = 0x08;
    idle_pcb.regs.ss      = 0x10;
}

/* ────────────────────────────────────────────────
   Public API
   ──────────────────────────────────────────────── */
void scheduler_init(void) {
    for (int i = 0; i < MAX_READY_QUEUE; i++) ready_queue[i] = (void*)0;
    queue_head  = 0;
    queue_tail  = 0;
    queue_count = 0;
    tick_counter = 0;
    current_ticks = 0;

    idle_pcb_init();
    current_process = &idle_pcb;

    klog(LOG_KERNEL, "Scheduler initialized (Round-Robin, 100ms slice)");
}

void scheduler_add_process(pcb_t* pcb) {
    if (!pcb) return;
    pcb->state = PROCESS_STATE_READY;
    queue_push(pcb);
}

void scheduler_remove_process(uint32_t pid) {
    /*
     * Physically remove queued entries before their PCB can be freed. Merely
     * marking a queued PCB terminated leaves a dangling pointer in the ring.
     */
    pcb_t* retained[MAX_READY_QUEUE];
    uint32_t retained_count = 0;

    while (queue_count) {
        pcb_t* candidate = queue_pop();
        if (!candidate) continue;
        if (candidate->pid == pid) {
            candidate->state = PROCESS_STATE_TERMINATED;
            continue;
        }
        retained[retained_count++] = candidate;
    }

    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    for (uint32_t i = 0; i < retained_count; ++i)
        queue_push(retained[i]);

    if (current_process && current_process->pid == pid)
        current_process->state = PROCESS_STATE_TERMINATED;
}

void scheduler_tick(void) {
    /* Reap only after the target's address space is no longer the active CR3. */
    process_reap_terminated();
    tick_counter++;
    current_ticks++;
    pcb_t** processes = process_table();
    for (uint32_t i = 0; i < MAX_PROCESSES; ++i) {
        pcb_t* p = processes[i];
        if (p && p->state == PROCESS_STATE_SLEEPING && p->wake_tick <= current_ticks) {
            p->wake_tick = 0;
            p->state = PROCESS_STATE_READY;
            queue_push(p);
        }
    }

    /* Only switch every TIME_SLICE_TICKS ticks */
    if (tick_counter < TIME_SLICE_TICKS) return;
    tick_counter = 0;

    /* Preemptive switch */
    scheduler_yield();
}

void scheduler_yield(void) {
    pcb_t* old = current_process;
    pcb_t* next = (void*)0;

    /* Drain terminated entries then pop next READY process */
    while (queue_count > 0) {
        pcb_t* candidate = queue_pop();
        if (candidate && candidate->state == PROCESS_STATE_READY) {
            next = candidate;
            break;
        }
    }

    if (!next || next == old) {
        if (old && old != &idle_pcb && old->state == PROCESS_STATE_RUNNING) {
            /* No peer is ready; keep the current runnable process. */
            queue_push(old);
            return;
        }
        if (old == &idle_pcb) return;

        /*
         * A terminated/non-runnable current process must not continue on its
         * stack merely because the ready queue is empty. Fall back to idle,
         * which uses the kernel address space.
         */
        next = &idle_pcb;
    }

    /* Re-enqueue old process if still runnable */
    if (old && old != &idle_pcb && old->state == PROCESS_STATE_RUNNING) {
        old->state = PROCESS_STATE_READY;
        queue_push(old);
    }

    /* Transition states */
    next->state     = PROCESS_STATE_RUNNING;
    current_process = next;

    /* Idle uses the kernel root; process tasks use their private roots. */
    if (next->page_directory)
        vmm_switch_page_directory(next->page_directory);
    else
        vmm_switch_page_directory(vmm_get_kernel_page_directory());
    uint64_t kernel_stack_top = next->stack_base
        ? (uint64_t)(uintptr_t)next->stack_base + next->stack_size
        : (uint64_t)(uintptr_t)idle_stack + sizeof(idle_stack);
    gdt_set_kernel_stack(kernel_stack_top);

    /* Perform the actual register-level context switch */
    context_switch(&old->rsp, next->rsp);
}

pcb_t* scheduler_current(void) {
    return current_process;
}

void scheduler_set_algorithm(sched_algorithm_t algo) {
    active_algorithm = algo;
}

uint64_t scheduler_get_uptime_ticks(void) {
    return current_ticks;
}

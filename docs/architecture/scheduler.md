# Scheduler Architecture - Phoxtra OS v0.5.0-alpha

## Overview

Phoxtra OS uses a **preemptive Round-Robin task scheduler** driven by the PIT timer interrupt (IRQ0 at 100 Hz). The scheduler is designed for future extensibility — additional algorithms can be plugged in without redesigning the subsystem.

---

## Design Goals

- **Preemptive**: The timer forces a context switch every 10 ticks (100ms)
- **Extensible**: `sched_algorithm_t` enum supports future Priority, Fair, Real-time, and Security-aware scheduling
- **Safe**: `scheduler_yield()` guards against switching to `NULL` or to itself
- **Low-overhead**: Idle process executes `hlt` — CPU halts until next interrupt

---

## Architecture

```
Timer IRQ0 (100Hz)
      │
      ▼
timer_callback()
      │
      ▼
scheduler_tick()   ← called every tick, switches every TIME_SLICE_TICKS
      │
      ▼
scheduler_yield()
      │
      ├── Pop next READY process from ready_queue
      │
      ├── Re-enqueue current process (if still RUNNING)
      │
      └── context_switch(&old_rsp, new_rsp)
                │
                └── context.asm: save 6 callee-saved regs, swap RSP, restore, ret
```

---

## Ready Queue

- Fixed-size circular queue (capacity: 64 processes)
- FIFO ordering for Round-Robin fairness
- Terminated processes are silently skipped during dequeue

---

## Idle Process

When no READY process exists, the scheduler keeps the CPU in the current process context. A dedicated **idle task** (`hlt` loop) runs whenever nothing else can.

```c
static void idle_task(void) {
    while (1) { asm volatile("hlt"); }
}
```

The idle process uses a statically allocated 4 KB stack (no heap dependency).

---

## Time Slice

| Parameter | Value |
|---|---|
| Timer frequency | 100 Hz |
| Ticks per slice | 10 |
| Effective slice | **100ms** |

---

## Public API

```c
void     scheduler_init(void);
void     scheduler_add_process(pcb_t* pcb);
void     scheduler_remove_process(uint32_t pid);
void     scheduler_tick(void);          // Called by timer IRQ
void     scheduler_yield(void);         // Voluntary preemption
pcb_t*   scheduler_current(void);      // Returns running PCB
void     scheduler_set_algorithm(sched_algorithm_t algo);
uint64_t scheduler_get_uptime_ticks(void);
```

---

## Future Scheduling Algorithms (Prepared)

| Constant | Algorithm |
|---|---|
| `SCHED_ROUND_ROBIN` | Current — FIFO time-slice |
| `SCHED_PRIORITY` | Priority queue — higher priority runs first |
| `SCHED_FAIR` | Weighted fair queuing |
| `SCHED_REALTIME` | Hard deadline scheduling |
| `SCHED_SECURITY_AWARE` | Trust-level and identity-based scheduling |

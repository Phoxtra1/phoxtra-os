# Process Management Architecture - Phoxtra OS v0.5.0-alpha

## Overview

The Phoxtra OS process management system provides the foundation for multitasking. Every unit of execution is represented by a **Process Control Block (PCB)** and managed by the **Process Manager**.

---

## Process Control Block (PCB)

**Source**: `kernel/process/pcb.h`, `kernel/process/pcb.c`

Each process is described by a `pcb_t` structure:

| Field | Type | Description |
|---|---|---|
| `pid` | `uint32_t` | Unique process identifier |
| `parent_pid` | `uint32_t` | PID of the spawning process |
| `name` | `char[32]` | Human-readable process name |
| `state` | `process_state_t` | Current lifecycle state |
| `priority` | `uint32_t` | Scheduling priority (higher = more urgent) |
| `rsp` | `uint64_t` | Saved stack pointer for context switching |
| `rip` | `uint64_t` | Instruction pointer at time of switch |
| `regs` | `cpu_registers_t` | Full register snapshot |
| `page_directory` | `uint64_t` | Physical address of page table root |
| `security_context` | `uint32_t` | Security identity token (future use) |
| `creation_time` | `uint64_t` | Timer tick at process creation |
| `stack_base` | `void*` | Kernel stack base pointer |
| `stack_size` | `uint64_t` | Stack size in bytes (default: 16 KB) |

---

## Process States

```
   NEW
    │
    ▼
  READY ◄────── WAITING
    │               ▲
    ▼               │
 RUNNING ───────────┘
    │
    ▼
TERMINATED
    │
  (freed)
```

| State | Meaning |
|---|---|
| `NEW` | PCB allocated, not yet scheduled |
| `READY` | Queued — awaiting CPU time |
| `RUNNING` | Currently executing on CPU |
| `WAITING` | Blocked on I/O or event |
| `SLEEPING` | Timer-delayed, not eligible to run |
| `TERMINATED` | Execution finished, PCB awaiting cleanup |

---

## Process Table

- Max processes: **64** (configurable via `MAX_PROCESSES`)
- Stored in a flat array indexed by slot (not PID)
- PIDs are monotonically increasing, allocated by `process_create()`

---

## Public API

```c
void    process_init(void);
pcb_t*  process_create(const char* name, void (*entry)(void), uint32_t priority);
void    process_destroy(uint32_t pid);
void    process_pause(uint32_t pid);
void    process_resume(uint32_t pid);
pcb_t*  process_lookup(uint32_t pid);
uint32_t process_count(void);
pcb_t** process_table(void);
```

---

## Context Switch Stack Frame

`pcb_create()` sets up an initial kernel stack frame compatible with `context_switch()` in `context.asm`:

```
Stack layout (growing downward):
  [RSP+48] = entry function pointer  ← jumped to via ret
  [RSP+40] = 0 (r15)
  [RSP+32] = 0 (r14)
  [RSP+24] = 0 (r13)
  [RSP+16] = 0 (r12)
  [RSP+8]  = 0 (rbp)
  [RSP+0]  = 0 (rbx)  ← RSP points here on first switch
```

# System Call Framework - Phoxtra OS v0.5.0-alpha

## Overview

Phoxtra OS provides a system call gateway via **software interrupt `int 0x80`**, registered in the IDT. User-space programs (when implemented in v0.6.0+) will use this interface to request kernel services.

---

## System Call Numbers

| Number | Name | Description |
|---|---|---|
| `1` | `SYS_PRINT` | Write a string to the console |
| `2` | `SYS_EXIT` | Terminate the calling process |
| `3` | `SYS_YIELD` | Voluntarily yield CPU to the scheduler |
| `4` | `SYS_SLEEP` | Suspend process for a specified duration |
| `5` | `SYS_GETPID` | Return the calling process's PID |

---

## Calling Convention

```
Register  Purpose
───────────────────────────────
RAX       System call number (input) / Return value (output)
RDI       First argument
RSI       Second argument
RDX       Third argument
```

Invoke with: `int 0x80`

---

## Dispatcher Flow

```
User / Kernel code
    │
    ▼
int 0x80
    │
    ▼
IDT Gate 0x80 → ISR stub (isr.asm)
    │
    ▼
irq_handler dispatch → syscall_handler()
    │
    ▼
Dispatch table lookup (syscall_table[RAX])
    │
    ▼
Individual handler: sys_print / sys_exit / sys_yield / sys_sleep / sys_getpid
```

---

## Adding New System Calls

1. Define a constant in `syscall.h`: `#define SYS_MYSERVICE 6`
2. Implement `static void sys_myservice(struct registers* regs)` in `syscall.c`
3. Add the function pointer to `syscall_table[]` at index `6`

---

## User Mode Architecture (Prepared)

The GDT now contains Ring 3 descriptors:

| Selector | Privilege | Description |
|---|---|---|
| `0x08` | Ring 0 | Kernel Code Segment |
| `0x10` | Ring 0 | Kernel Data Segment |
| `0x1B` | Ring 3 | User Code Segment |
| `0x23` | Ring 3 | User Data Segment |

A `switch_to_user_mode()` gateway function will be implemented in v0.6.0 using `iretq` to transition from Ring 0 to Ring 3. The syscall framework is already in place to handle the return path.

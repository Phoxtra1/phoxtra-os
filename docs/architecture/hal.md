# Hardware Abstraction Layer (HAL) - Phoxtra OS v0.4.0-alpha

The Hardware Abstraction Layer (HAL) isolates low-level x86_64 architecture hardware implementations from high-level kernel logic and system services.

## HAL Layer Hierarchy

```
Applications
     |
System Services
     |
   Kernel
     |
    HAL  <-- Abstract Hardware Table
     |
  Hardware (CPU, PIT Timer, PS/2 Keyboard, VGA Console)
```

## Device Interfaces

The HAL maintains function pointer tables for:
- **CPU Operations**: `disable_interrupts`, `enable_interrupts`, `halt`
- **Timer Operations**: `init`, `get_ticks`
- **Keyboard Operations**: `init`, `get_char`
- **Display Operations**: `clear`, `write`, `put_char`

## Interface Functions

```c
#include "hal.h"

hal_init();
uint64_t ticks = hal_timer_get_ticks();
char input = hal_keyboard_get_char();
hal_display_write("Message via HAL");
```

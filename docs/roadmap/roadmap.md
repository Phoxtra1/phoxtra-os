# Phoxtra OS Development Roadmap

This document outlines the evolutionary steps needed to transition Phoxtra OS from its current minimal bootable prototype (v0.1.0-alpha) to a secure, identity-first, production-ready operating system.

---

## 🗺️ Phase-by-Phase Roadmap

### Phase 1: Kernel Foundation & Core Hardware (Current Focus)
The objective of this phase is to establish a stable, safe environment inside Ring 0 with basic hardware drivers, interrupts, memory management, and diagnostics.

1. **Interrupt Vector Table (IDT)**
   - Define structure of IDT entries and register the table using `lidt`.
   - Setup assembly stub handlers for common CPU exceptions (Page Fault, Double Fault, General Protection Fault).
   - Implement Interrupt Service Routines (ISRs) and map the programmable interrupt controller (PIC/APIC) to handle hardware interrupts (Timer, Keyboard).

2. **Physical Page Allocator**
   - Parse Multiboot 2 memory map tags to discover available RAM regions.
   - Implement a **Bitmap Allocator** or **Buddy Allocator** to allocate and free physical frames (4KB pages).

3. **Virtual Memory Manager & Kernel Heap**
   - Create a proper Paging module to dynamically map virtual pages to physical frames.
   - Map a kernel heap region and implement basic dynamic allocation (`kmalloc` and `kfree`).
   - Implement memory safety checks to guard against buffer overflows or unauthorized memory access.

4. **Panic and Diagnostics Module (`kernel/src/panic.cpp`)**
   - Implement a robust `kernel_panic` function that prints diagnostic registers, stack traces, and halts the CPU safely.
   - Standardize `ASSERT()` macros across all kernel sources.

---

### Phase 2: Multitasking & User-Space (Ring 3)
The objective of this phase is to support multiple parallel tasks (threads and processes) and transition safely from kernel space (Ring 0) to user space (Ring 3).

1. **Process & Thread Representation**
   - Design the Thread Control Block (TCB) and Process Control Block (PCB).
   - Implement task context save/restore routines in assembly.

2. **Preemptive Scheduler**
   - Implement a simple preemptive scheduler (e.g., Round-Robin with priority levels).
   - Bind the PIT (Programmable Interval Timer) or APIC timer to invoke the scheduler periodically.

3. **User-Space Transition & System Calls**
   - Set up the GDT with User Code and User Data segments.
   - Implement a system call mechanism (via `syscall`/`sysret` instruction on x86_64).
   - Provide basic system calls: `sys_exit`, `sys_write` (to console), `sys_yield`, and `sys_fork`.

4. **Kernel Synchronization Primitives**
   - Implement Spinlocks (for interrupt/multicore safety) and Mutexes/Semaphores (for thread blocking).

---

### Phase 3: Filesystem, Drivers & Device Management
The objective is to allow Phoxtra OS to interact with storage, input devices, and provide a file interface.

1. **Core Device Drivers**
   - Implement a PS/2 Keyboard driver.
   - Implement a storage driver (e.g., AHCI for SATA, or IDE/RAMDisk) to read/write disk blocks.

2. **Virtual Filesystem (VFS) & Disk FS**
   - Implement a unified VFS abstraction (supporting `open`, `read`, `write`, `close`).
   - Implement a simple read/write filesystem driver (such as FAT32, or a custom secure filesystem).

3. **ELF Executable Loader**
   - Write a loader that parses 64-bit ELF executable files from disk, loads segments into memory, sets up the user stack, and jumps to the user entry point.

---

### Phase 4: Phoxtra Shield & Identity Platform (Enterprise & Security Core)
This phase introduces the primary unique selling points of Phoxtra OS—Identity, security protection, and verifiable computing.

1. **Phoxtra Shield: Security Core**
   - Enforce page-level execution protection (NX bit).
   - Implement Address Space Layout Randomization (ASLR) for user space executables.
   - Add application-integrity verification during ELF load (checking digital signatures against system keys).

2. **PhoxtraID Platform**
   - Design secure credential storage isolated from standard user-space applications (TPM-backed secure storage or hypervisor/microkernel enclave emulation).
   - Setup passwordless/biometric/passkey validation framework at the OS login boundary.

3. **Verify Platform**
   - Native verification APIs inside userspace to validate documents, certificates, and credentials.

---

### Phase 5: Operating System Services & User Experience
The objective of this phase is to build the visual and interactive layers for end-users and developers.

1. **System Config & Boot Options**
   - Populate `config/*.yml` and parser inside the bootloader/kernel to configure startup policies, security parameters, and screen resolutions.

2. **OdeAI Agent & Diagnostics Integration**
   - Build a lightweight native assistant running as a user-space daemon to parse logs, run active diagnostic tests, and offer system administration advice.

3. **Userspace Window Manager & Shell**
   - Implement a graphic frame-buffer driver.
   - Build a minimal GUI window manager and an interactive terminal shell.

4. **Developer SDK**
   - Publish developer header files, C standard library port (e.g., `newlib` or `musl`), and standard compiler targets.

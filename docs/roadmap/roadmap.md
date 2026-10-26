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

**Current status:** Experimental read-only physical storage and a limited on-disk ext2 reader are implemented and exercised by automated QEMU tests. This does not mean the physical laptop is validated or that writable persistent storage is available.

1. **Core Device Drivers**
   - The PS/2 keyboard and mouse paths are present and tested in the declared prototype scope.
   - Experimental PCI AHCI/SATA support identifies supported ATA LBA48 disks with 512-byte logical sectors and performs bounded read-only sector I/O.
   - NVMe, non-512-byte logical sectors, full device compatibility, and physical-laptop driver acceptance remain unimplemented or unverified.

2. **Virtual Filesystem (VFS) & Disk FS**
   - The VFS exposes supported files from a limited read-only ext2 subset through directory and file reads.
   - MBR partition discovery validates bounds and overlap, skips unsupported extended-partition chains, and exposes discovered partitions read-only.
   - The ext2 parser rejects journaled filesystems, indexed directories, malformed structures, and unsupported feature combinations.
   - Writable filesystem support, on-disk FAT32, flush/write semantics, filesystem repair, and write-based recovery remain unimplemented. The simulated RAM demo remains volatile.

3. **ELF Executable Loader**
   - ELF64 structural validation is implemented, but loading segments into isolated process address spaces and executing them remain disabled.

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

**Current status:** A limited native Settings panel is implemented. It supports desktop-theme selection and read-only display/input/storage readiness reporting. System-wide configuration, persistent user preferences, and general resolution, network, account, sound, power, and update controls remain planned.

1. **System Config & Boot Options**
   - Populate `config/*.yml` and parser inside the bootloader/kernel to configure startup policies, security parameters, and screen resolutions.
   - Expand the native Settings panel only as the underlying configuration APIs and device services become implemented and tested.

2. **OdeAI Agent & Diagnostics Integration**
   - Build a lightweight native assistant running as a user-space daemon to parse logs, run active diagnostic tests, and offer system administration advice.

3. **Userspace Window Manager & Shell**
   - Implement a graphic frame-buffer driver.
   - Build a minimal GUI window manager and an interactive terminal shell.

4. **Developer SDK**
   - Publish developer header files, C standard library port (e.g., `newlib` or `musl`), and standard compiler targets.

---

### Phase 6: Application Environment & Integrated Workspace
The objective is to make Phoxtra a usable operating system, not just a bootable kernel or desktop demo. Users must be able to discover, launch, use, and manage supported applications through a coherent application environment.

1. **Phoxtra Application Access**
   - Build the application launcher/catalogue and a consistent way for users to find, open, close, and manage installed applications.
   - Define application packaging, installation/update flow, launch permissions, and clear handling for unsupported or failed applications.
   - Make the environment ready for applications developed specifically for Phoxtra as the user-space runtime and executable-loading capabilities mature.

2. **Integrated Amineva Global Applications**
   - Establish the integration model for supported Amineva Global applications and services.
   - Provide clear entry points for integrated applications without presenting web links as if they were native apps; use the appropriate native, web, or service integration for each product.
   - Keep application integration modular so individual products can be added, updated, or removed without destabilizing the OS.

3. **Amineva Workspace**
   - Include Amineva Workspace as a first-class workspace experience in the Phoxtra environment.
   - Define how users access its supported productivity tools and shared services, including how sign-in, files, and application hand-off work where those capabilities are implemented.
   - Do not claim offline support, cross-app file access, or account integration is complete until each capability is implemented and tested.

4. **Application Security & Compatibility**
   - Connect application launch to Phoxtra Shield's permission and integrity controls as those controls become available.
   - Define supported application interfaces and compatibility expectations; provide useful error messages and logs for application failures.
   - Test the application launcher and integrated apps under normal use, restart, and failure conditions.

---

### Phase 7: Complete OS Validation on a Standard Laptop
**This is a mandatory release gate before Phoxtra is placed on dedicated Phoxtra-designed hardware. It must not be skipped or replaced by a successful ISO build alone.** Use a standard laptop as the real-machine validation target after the OS foundation, desktop, storage, and planned application environment are ready enough for end-to-end testing.

1. **Boot & Hardware Compatibility**
   - Boot from the prepared installation/live media on a standard laptop, not only in QEMU.
   - Verify boot reliability, display modes, keyboard, pointer/touchpad where supported, storage detection, reboot and shutdown behaviour.
   - Record hardware-specific limitations and distinguish unsupported devices from OS defects.

2. **Desktop & Everyday Interaction**
   - Verify login/session entry where implemented, desktop rendering, pointer movement without trails or artifacts, keyboard input, icons, folders, file manager, window movement and controls, taskbar, themes, and settings.
   - Exercise file operations and persistence across restart when disk-backed storage is implemented.
   - Check stability over repeated boots and extended interactive use.

3. **System & Application End-to-End Tests**
   - Exercise kernel diagnostics, memory management, interrupt/input handling, scheduling and user-space execution as each becomes implemented.
   - Launch and use supported Phoxtra applications, integrated Amineva Global applications, and Amineva Workspace features that are included in the build.
   - Check failure recovery, logs, permissions, storage behaviour, and regressions after updates.

4. **Acceptance Record**
   - Keep a test checklist with laptop model/specification, boot method, working features, defects, workarounds, and retest results.
   - Fix release-blocking defects and repeat failed tests before declaring the laptop milestone passed.
   - Do not begin the first dedicated Phoxtra hardware integration until this gate passes; laptop testing is required evidence, not an optional later task.

---

### Phase 8: First Dedicated Phoxtra Hardware Integration

**Current substage: software readiness and integration preparation in progress.** The read-only `hardware` shell report inventories the active x86_64/Multiboot2 profile, display/input availability, PCI device classes, simulated storage, and whether real block-device/persistent filesystem services are registered. It must not be mistaken for physical-device support.

Continue preparation and automated validation while Phase 7 physical testing is pending. The dedicated-board bring-up gate remains: do not start target-specific board drivers or claim Phase 7 passed until the standard-laptop acceptance checklist has real test evidence. Before hardware bring-up, select a target board and record firmware, CPU, memory, display, input, storage, power/thermal, peripherals, and recovery requirements. Preserve read-only storage discovery until a separately reviewed safe storage design exists.

See [the Phase 8 hardware integration plan](phase8-hardware-integration-plan.md) for current implementation status and the required gate.

---

## Roadmap Integrity Rules
- The phases above are cumulative: later phases do not erase earlier work or bypass unfinished prerequisites.
- Keep desktop polish and ISO/QEMU checks as early verification steps, but do not treat them as a substitute for a complete laptop test.
- Track each milestone as **not started**, **in progress**, **blocked**, or **passed** based on evidence. Never mark a build, boot, application, or hardware test as passed unless it was actually run and its result recorded.
- Dedicated Phoxtra hardware comes after the full standard-laptop validation gate. The intended end state includes a usable application environment, Phoxtra applications, integrated Amineva Global applications, and Amineva Workspace.


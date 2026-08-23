# Changelog

All notable changes to this project will be documented in this file.

## [v1.0.0] - 2026-08-22

### Added
- **Official Commercial Release Distribution Artifacts (`scripts/build-release-artifacts.sh`)**:
  - `phoxtra-os-v1.0.0.iso`: Bootable Hybrid ISO image.
  - `phoxtra-os-v1.0.0.img`: Live USB Installer raw disk image.
  - `phoxtra-os-v1.0.0.qcow2`: QEMU/KVM Virtual Cloud Disk image.
  - `phoxtra-os-v1.0.0.vhd`: AWS AMI & Hyper-V Cloud VHD image.
- **Bare-Metal Hardware Certification Suite (`kernel/release/production.c`, `production.h`)**:
  - Compatibility verification across modern Intel Core and AMD Ryzen x86_64 platforms, hardware TPM 2.0 SPI, VBE/GOP 1024x768 framebuffer, and e1000 Gigabit networking.
- **Enterprise Support & Maintenance (`docs/enterprise/ENTERPRISE_LTS.md`)**:
  - 5-Year Enterprise Long Term Support (LTS) policy (2026–2031).
  - Security Vulnerability Response Pipeline (PSRT) & CVE hotpatching SLA.
- **Interactive Shell Command**:
  - Added `release` / `lts` command to display commercial launch status and Enterprise LTS guarantees.

## [v0.9.9-RC3] - 2026-08-22

### Added
- **Phoxtra OS Developer Cross-Compiler Toolchain (`sdk/tools/phoxtra-gcc`)**:
  - `x86_64-phoxtra-elf-gcc` target wrapper configuring sysroot and standard library header paths.
- **Phoxtra SDK Project Management Utility (`sdk/tools/phoxtra-sdk`)**:
  - Command-line tool supporting `phoxtra-sdk init`, `phoxtra-sdk build`, and `phoxtra-sdk deploy`.
- **Developer Documentation & Reference Manuals**:
  - `docs/developer/DEVELOPER_GUIDE.md`: Comprehensive manual for building and deploying Ring 3 applications.
  - `docs/developer/SYSCALL_REFERENCE.md`: Complete register calling convention and 64-system-call API table.
  - `docs/developer/ARCHITECTURE_MANUAL.md`: System architecture manual covering hardware root-of-trust, network stack, VBE compositor, and dynamic runtime.
- **In-Kernel SDK Subsystem (`kernel/sdk/sdk.c`, `sdk.h`)**:
  - Subsystem initialization and version reporting gateway.
- **Interactive Shell Command**:
  - Added `sdk` / `toolchain` command to audit Phoxtra Developer SDK state, toolchain wrappers, and syscall gateway status.

## [v0.9.9-RC2] - 2026-08-22

### Added
- **Preemption Multitasking Stress Engine (`kernel/process/stress.c`, `stress.h`)**:
  - Maximum process capacity stress testing managing up to 64 concurrent tasks.
  - 100Hz PIT timer preemption stability verification.
- **Kernel Memory Leak Auditor (`kernel/memory/heap_audit.c`, `heap_audit.h`)**:
  - Physical allocator & heap chunk tracking verifying 0 orphaned pointers and 0 memory leaks.
- **Panic Resilience & Edge-Case Fault Gate**:
  - Ring 0 General Protection Fault (#GP), Page Fault (#PF), and Double Fault (#DF) panic recovery hooks.
- **Interactive Shell Commands**:
  - Added `stress` command to run multitasking preemption tests under 64-task load.
  - Added `heap` command to execute kernel memory leak and heap integrity audits.
  - Added `panic` command to audit kernel exception interception and fault recovery state.

## [v0.9.9-RC1] - 2026-08-22

### Added
- **VESA VBE / UEFI GOP Linear Framebuffer Driver (`kernel/drivers/framebuffer.c`, `framebuffer.h`)**:
  - High-resolution 1024x768 @ 32bpp ARGB8888 graphics mode initialization.
  - Linear framebuffer abstraction with double-buffering (`fb_swap_buffers`) to eliminate screen tearing.
  - Accelerated drawing primitives: `fb_put_pixel`, `fb_draw_rect`, `fb_fill_rect`, `fb_draw_gradient`, and `fb_clear`.
- **Bitmap Font Engine (`kernel/ui/font.c`, `font.h`)**:
  - 8x16 anti-aliased bitmap glyph table for high-legibility UI text rendering (`font_draw_char`, `font_draw_string`).
- **Window Compositor Engine (`kernel/ui/compositor.c`, `compositor.h`)**:
  - Double-buffering window compositor supporting glassmorphism aesthetics, window drop shadows, active title bars, and close/minimize buttons.
  - Taskbar / Dock at bottom of screen with system clock, CPU load, and launcher icons.
- **Phoxtra Desktop Window Manager (`phoxtra-wm`) (`kernel/ui/wm.c`, `wm.h`)**:
  - Desktop environment managing default windows: Terminal (Ring 3), System Performance Monitor, and Security Audit Dashboard.
- **Interactive GUI Shell Commands**:
  - Added `gui` / `desktop` command to launch high-resolution 1024x768 desktop interface.
  - Added `wm` command to audit window compositor state and screen refresh metrics.

## [v0.9.5-alpha] - 2026-08-22

### Added
- **ELF64 Executable Loader (`kernel/exec/elf.c`, `elf.h`)**:
  - `\x7fELF` magic validation, AMD x86-64 machine checking, and 64-bit LSB class confirmation.
  - Program header parsing (`elf64_phdr_t`) for `PT_LOAD` segments, memory allocation, and virtual mapping.
  - Entry point (`e_entry`) extraction (`elf_load_binary`).
- **Dynamic Linker & Shared Library Loader (`kernel/exec/ld_so.c`, `ld_so.h`)**:
  - Dynamic symbol lookup and shared library (`.so`) resolution (`ld_so_resolve_symbol`).
  - Runtime link map initialization (`/lib64/ld-phoxtra.so.1` & `libc.so`).
- **Native C Standard Library (`phoxtra-libc`) POSIX Subset (`libc/include/`, `libc/src/libc.c`)**:
  - Standard headers: `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`.
  - Implemented core C runtime: `printf`, `puts`, `putchar`, `getchar`, `fopen`, `fclose`, `fread`, `fwrite`, `malloc`, `free`, `exit`, `atoi`, `abs`, `strlen`, `strcpy`, `strncpy`, `strcmp`, `strncmp`, `memset`, `memcpy`, `read`, `write`, `open`, `close`, `getpid`, `usleep`, `fork`, `execve`.
  - Syscall gateway wrappers routing through x86_64 `syscall` instruction into kernel handlers.
- **Diagnostic Shell Commands**:
  - Added `elf <path>` command to parse, validate, and load ELF64 binaries from VFS.
  - Added `libc` command to audit native C standard library routines and POSIX syscall state.

## [v0.9.0-alpha] - 2026-08-22

### Added
- **Intel 82540EM Gigabit Ethernet NIC Driver (`kernel/net/e1000.c`)**:
  - Probes e1000 PCI network interface controller (`0x8086:0x100E`).
  - Implemented 32-descriptor RX and TX ring buffers and MAC address readout (`52:54:00:12:34:56`).
- **Ethernet II, ARP & IPv4 Packet Processing (`kernel/net/ethernet.c`, `arp.c`, `ipv4.c`)**:
  - Ethernet II frame encapsulation with EtherTypes `0x0800` (IPv4) and `0x0806` (ARP).
  - ARP table cache (`32-entry`) and ARP request/reply generation.
  - IPv4 packet header parsing, checksum engine (`net_checksum`), and ICMP Echo ping subsystem.
- **UDP & TCP Transport Protocols with BSD Socket API (`kernel/net/udp.c`, `tcp.c`, `socket.c`)**:
  - UDP datagram formatting and port multiplexing.
  - TCP finite state machine (`CLOSED`, `LISTEN`, `SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`, `FIN_WAIT`).
  - BSD Socket API functions: `sys_socket`, `sys_bind`, `sys_connect`, `sys_listen`, `sys_send`, `sys_recv`.
- **DHCP Client & DNS Resolver (`kernel/net/dhcp.c`, `dns.c`)**:
  - DHCP DISCOVER / OFFER / REQUEST / ACK state machine for auto-assigning IP (`192.168.1.150`), netmask (`255.255.255.0`), gateway (`192.168.1.1`), and DNS (`8.8.8.8`).
  - DNS host resolution service for mapping domain names to IPv4 addresses.
- **Interactive Network Commands**:
  - Added `ifconfig` command to inspect network interface state, IP configuration, and packet statistics.
  - Added `ping` command to test connectivity to IPv4 target hosts.
  - Added `netstat` command to view active BSD sockets and connection states.
  - Added `dhcp` command to request dynamic IP leases.
  - Added `dns` command to resolve hostnames.

## [v0.8.5-alpha] - 2026-08-22

### Added
- **Capability Access Control (CAC) Subsystem (`kernel/security/capability.c`)**:
  - Replaced legacy UID/GID root permissions with fine-grained 64-bit capability bitmask sets (`CAP_SYS_ADMIN`, `CAP_DEV_ACCESS`, `CAP_IPC_SEND`, `CAP_FS_MOUNT`, `CAP_PROCESS_KILL`, `CAP_SECURITY_AUDIT`).
  - Added process capability assignment and permission enforcement (`capability_check`).
- **Mandatory Access Control (MAC) Policy Framework (`kernel/security/mac.c`)**:
  - Domain isolation labels: `SYSTEM`, `SERVICE`, `USER`, and `RESTRICTED` sandbox processes.
  - Non-discretionary VFS access rules and IPC cross-domain message filtering.
- **Kernel Stack Protection & Security Guards (`kernel/security/stack_guard.c`)**:
  - Stack Canary verification (`__stack_chk_guard`) initialized from TPM 2.0 hardware entropy.
  - GCC stack overflow trap handler (`__stack_chk_fail`) logging policy violation and halting process.
  - 4KB unmapped Kernel Guard Page boundaries at process stack frames.
  - ASLR offset generation for base address randomization.
- **Security Diagnostic Commands**:
  - Added `caps` command to inspect capability sets across all running PIDs.
  - Added `mac` command to review Mandatory Access Control domain rules.
  - Added `stackguard` command to audit stack canary seeds, guard pages, and ASLR state.

## [v0.8.0-alpha] - 2026-08-22

### Added
- **Phoxtra Identity Engine — TPM 2.0 SPI Hardware Root of Trust (`kernel/security/tpm2.c`)**:
  - Implemented TPM 2.0 SPI MMIO interface (`0xFED40000`) and Platform Configuration Register (PCR) subsystem.
  - Initialized hardware root of trust measurements: PCR0 (Bootloader), PCR1 (Phoxtra Kernel Core), PCR2 (System Config), PCR3 (Process Table).
  - Attached Endorsement Key (EK) hash generation and hardware attestation quote signing.
- **Cryptographic Process Identity Attestation (`kernel/security/process_identity.c`, `sha256.c`)**:
  - Bound every Process Control Block (`pcb_t`) to a unique SHA-256 process digest and authority cryptographic signature.
  - Integrated process signature validation (`process_identity_verify`) on process execution and context switch.
  - Automatically extend TPM 2.0 PCR3 on process creation.
- **Immutable Security Event Audit Logger (`kernel/security/audit_logger.c`)**:
  - 128-entry kernel ring buffer recording all hardware attestations, process bindings, privilege transitions, and syscall invocations.
  - Cryptographic SHA-256 hash chaining sealing each audit log entry to guarantee immutability.
- **Interactive Security Diagnostics**:
  - Added `tpm` command to inspect TPM 2.0 status, vendor ID, and PCR 0-3 digests.
  - Added `identity` command to display active process PIDs, verification states, and SHA-256 hashes.
  - Added `audit` command to view the immutable kernel security event audit log chain.

## [v0.7.0-alpha] - 2026-08-22

### Added
- **Storage Block Device Subsystem (`kernel/storage/blockdev.c`)**:
  - Implemented `block_device_t` uniform block interface (`read_sectors`, `write_sectors`, sector sizing, flags).
  - Created global block device registry supporting master drives and logical partitions.
- **AHCI SATA / NVMe Controller Driver (`kernel/storage/ahci.c`)**:
  - Added AHCI PCI Host Bus Adapter probing routines.
  - Implemented ATA DMA sector read/write execution pipelines.
  - Attached `/dev/sata0` 32MB LBA storage drive.
- **MBR & GPT Partition Table Parser (`kernel/storage/partition.c`)**:
  - Added MBR signature (`0xAA55`) and 4-slot primary partition entry parser.
  - Added GPT GUID Header (`EFI PART`) structure definitions.
  - Automatic creation of logical partition block devices (`sata0p1`, `sata0p2`, `sata0p3`).
- **Ext2 & FAT32 Block Filesystem Drivers (`kernel/storage/ext2.c`, `kernel/storage/fat32.c`)**:
  - Ext2 Superblock (`0xEF53`) verification and volume mounting for `/boot` (Ext2) and `/usr` (Ext2).
  - FAT32 BPB parameter block parser and volume mounting for `/home` (FAT32).
  - Full integration into `vfs_node_t` hierarchy.
- **Interactive Storage Shell Diagnostics**:
  - Added `disks` command for listing registered block storage devices, sizes, and partition tables.
  - Added `storage` command for auditing AHCI probing, partition tables, and Ext2/FAT32 volume mounts.

## [v0.6.0-alpha] - 2026-08-22

### Added
- **Ring 3 Privilege Separation & Execution Gateway**:
  - Expanded GDT to include Ring 3 User Code (`0x1B`) and User Data (`0x23`) segment descriptors.
  - Configured 64-bit Task State Segment (TSS) at selector `0x28` loaded via `ltr`.
  - Implemented `switch_to_user_mode(entry, rsp)` assembly gateway utilizing `iretq` stack frames.
- **Hardware Fast System Calls (`syscall`/`sysret`)**:
  - Configured Model-Specific Registers (`IA32_EFER`, `IA32_STAR`, `IA32_LSTAR`, `IA32_FMASK`).
  - Added low-overhead assembly entry `syscall_entry` bypassing traditional IDT interrupt gates.
- **Per-Process Address Space Isolation**:
  - Added `vmm_create_user_page_directory()` to clone identity kernel mappings into isolated PML4 page tables for each PCB.
  - Integrated automatic `CR3` page directory switching into `scheduler_yield()` context switch.
- **Virtual File System (VFS) & Ramdisk Abstraction (v0.6.5 baseline)**:
  - Created translucent `vfs_node_t` abstraction supporting standard VFS operations (`open`, `read`, `write`, `close`, `readdir`).
  - Implemented `devfs` character device filesystem (`/dev/console`, `/dev/keyboard`, `/dev/null`).
  - Implemented `initrd` initial ramdisk filesystem mounted at `/initrd` (`/initrd/welcome.txt`, `/initrd/system.cfg`).
- **Interactive Shell Diagnostics**:
  - Added `usermode` / `testuser` diagnostic command to verify privilege transitions and memory isolation boundaries.
  - Added `vfs` / `ls` directory enumeration command.
  - Added `cat <path>` command for reading VFS file nodes.

## [v0.5.2-alpha] - 2026-07-21


### Fixed
- **Interactive Shell Regression (keyboard input not accepted)**: Root cause identified and resolved.

  **Root cause**: In v0.5.0-alpha, `init.cpp` called `scheduler_add_process()` for the `kernel`, `logger`, and `security` tasks. These tasks were placed in the scheduler's ready queue. When the PIT timer fired (IRQ0 at 100 Hz), `scheduler_tick()` → `scheduler_yield()` found READY processes in the queue and performed a `context_switch()`. This permanently transferred the CPU from the shell's execution context (running on the boot stack) to one of the `hlt`-loop stubs — abandoning `keyboard_get_char()` mid-wait and making the shell unresponsive.

  **Fix**: Kernel system tasks (`kernel`, `logger`, `security`, `shell`) are now **registered in the process table only** — they are **not enqueued** in the scheduler's ready queue. Their state is set to `WAITING` (or `RUNNING` for shell) so they appear correctly in `ps`. With an empty ready queue, `scheduler_yield()` returns immediately at its early-exit guard (`!next || next == old`) without performing any context switch. The timer interrupt returns cleanly to the shell's blocking `keyboard_get_char()` loop on every tick.

  **Event pipeline restored**:
  ```
  Keyboard key press
      ↓
  IRQ1 → keyboard_callback()
      ↓
  scancode → ASCII → kbd_buffer[kbd_head++]
      ↓
  keyboard_get_char() unblocks (kbd_head != kbd_tail)
      ↓
  shell input loop processes character
      ↓
  Command parser → shell response
  ```

- **Process table populated correctly**: Added `shell` as a `RUNNING` process and `kernel`/`logger`/`security` as `WAITING` so `phoxtra> ps` now shows all 4 entries with accurate states.

- **SSE/AVX auto-vectorization crash fixed**: The compiler was emitting XMM/SSE2 instructions (e.g. `movdqu`, `movdqa`) to optimize loops in `idt_init()`. These instructions require `CR4.OSFXSR` to be set, which the boot stub never does. Executing them before the IDT is loaded caused a `#UD` Device-Not-Available exception → no handler → triple fault → CPU reset. Fixed by adding `-mno-sse -mno-sse2 -mno-avx -mno-avx2` to `CMakeLists.txt` compile options.

### Changed
- `kernel/src/init.cpp`: Removed `scheduler_add_process()` calls. Added `shell_task` stub. Process states set explicitly after creation.
- `kernel/CMakeLists.txt`: Added `-mno-sse -mno-sse2 -mno-avx -mno-avx2` compile flags to prevent auto-vectorization crashes in the pre-FPU kernel environment. Reverted GDT to 3 entries (Ring 3 segments deferred to v0.6.0).
- `kernel/arch/x86_64/context.asm`: Fixed `.note.GNU-stack` section flags (`nowrite` instead of `write`).
- `kernel/arch/x86_64/gdt/gdt.cpp`: Reverted to 3-entry GDT (Ring 3 user-mode segments deferred to v0.6.0 when `iretq` transition is implemented).

## [v0.5.1-alpha] - 2026-07-21

### Fixed
- **Boot Identity Banner Restored**: The Phoxtra OS identity banner (`=====…=====` border, OS name, tagline, and version) disappeared in v0.5.0-alpha because `kernel_init` stopped writing console output before calling `system_init`, and `shell_start` used plain unformatted text. The banner is now rendered correctly on every boot via the dedicated `boot_screen` module (see below).

### Added
- **Boot Screen Module** (`kernel/src/boot_screen.h`, `kernel/src/boot_screen.c`): New modular boot presentation layer. Encapsulates all visual boot output behind four clean functions:
  - `boot_screen_banner()` — clears the display and renders the full identity banner.
  - `boot_screen_section_boot()` — prints the `[BOOT]` section header.
  - `boot_screen_section_kernel()` — prints the `[KERNEL]` section header.
  - `boot_screen_ready()` — prints `System Ready` and spacing before the shell prompt.

### Changed
- `kernel/src/shell.c`: Replaced all inline banner/section strings in `shell_start()` with calls to `boot_screen_*`. No logic changes — only presentation is affected.
- `kernel/CMakeLists.txt`: Added `src/boot_screen.c` to the kernel source list.

### Boot Presentation (v0.5.1-alpha)
```
=================================================
               PHOXTRA OS
      Secure Identity Operating System
               v0.5.1-alpha
=================================================

[BOOT]

CPU .............. OK
GDT .............. OK
Kernel ........... OK
IDT .............. OK
PIC .............. OK
Timer ............ OK
Keyboard ......... OK

[KERNEL]

Logger ........... OK
Memory ........... OK
Paging ........... OK
Heap ............. OK
HAL .............. OK
Scheduler ........ OK
Processes ........ OK
Syscalls ......... OK
Security ......... OK

System Ready


phoxtra>
```

## [v0.5.0-alpha] - 2026-07-21

### Added
- **Process Control Block (PCB)**: Defined `pcb_t` with PID, name, state, priority, RSP, RIP, register snapshot, parent PID, page directory, security context, and creation time (`process/pcb.h`, `process/pcb.c`).
- **Process States**: Six lifecycle states — `NEW`, `READY`, `RUNNING`, `WAITING`, `SLEEPING`, `TERMINATED`.
- **Process Manager**: PID allocator, 64-slot process table, and full lifecycle API — `process_create()`, `process_destroy()`, `process_pause()`, `process_resume()`, `process_lookup()` (`process/process.h`, `process/process.c`).
- **Round-Robin Task Scheduler**: Preemptive scheduler with circular ready queue, 100ms time slice, idle task support, and extensible algorithm interface (`scheduler/scheduler.h`, `scheduler/scheduler.c`).
- **x86_64 Context Switch**: Assembly-level register save/restore via `context_switch()` — pushes and pops 6 callee-saved registers (rbx, rbp, r12–r15) and swaps RSP (`arch/x86_64/context.asm`).
- **Timer-Driven Preemption**: `timer_callback()` now calls `scheduler_tick()` on every PIT IRQ0, triggering time-slice switching.
- **Kernel Idle Process**: Permanent `hlt`-loop idle task using a statically allocated 4KB stack. Runs when no other process is READY.
- **Kernel System Tasks**: `kernel`, `logger`, and `security` registered as process-table entries visible to the scheduler.
- **User Mode Foundation**: GDT expanded from 3 to 5 entries — Ring 3 user code (`0x1B`) and data (`0x23`) segment descriptors now loaded at boot.
- **System Call Framework**: `syscall_handler()` dispatched from IDT/IRQ path, supporting `SYS_PRINT` (1), `SYS_EXIT` (2), `SYS_YIELD` (3), `SYS_SLEEP` (4), `SYS_GETPID` (5) (`syscall/syscall.h`, `syscall/syscall.c`).
- **Kernel Event System**: Publish-subscribe message bus with 32 event types and 16 subscribers per type — allows decoupled inter-subsystem communication (`events/events.h`, `events/events.c`).
- **Shell Process Diagnostics**: New shell commands — `ps` (process table), `pid` (current PID), `memory` (PMM stats), `uptime` (scheduler ticks), `cpu` (scheduler state).
- **Updated Startup Sequence**: Shell now displays full `[BOOT]` + `[KERNEL]` checklist including Scheduler, Processes, Syscalls, Security, followed by `System Ready`.

### Changed
- `kernel/src/timer.c`: Replaced raw serial output with `klog()`. Removed unused `serial_write_dec` helper.
- `kernel/src/init.cpp`: Fully rewritten to initialize all subsystems in strict dependency order.
- `kernel/src/shell.c`: Fully rewritten with v0.5.0-alpha startup screen and extended diagnostic commands.
- `kernel/arch/x86_64/gdt/gdt.cpp`: GDT expanded to 5 entries to include Ring 3 user-mode segments.
- `kernel/CMakeLists.txt`: Added `context.asm`, `process/`, `scheduler/`, `syscall/`, `events/` to build system.

## [v0.4.0-alpha] - 2026-07-21

### Added
- **Kernel Logging System**: Created centralized level-based logging system (`logger.c`, `logger.h`) supporting `INFO`, `WARNING`, `ERROR`, `SECURITY`, and `KERNEL` levels.
- **Physical Memory Manager (PMM)**: Implemented 4KB physical page bitmap allocator (`pmm.c`, `pmm.h`) with Multiboot2 memory detection.
- **Virtual Memory Manager (VMM)**: Implemented x86_64 4-level paging (`vmm.c`, `vmm.h`) for PML4, PDPT, PDT, and PT page table traversal and virtual page mapping.
- **Kernel Heap Allocator**: Added dynamic memory allocation (`heap.c`, `heap.h`) implementing `kmalloc()` and `kfree()`.
- **Hardware Abstraction Layer (HAL)**: Added HAL foundation (`hal.c`, `hal.h`) to decouple CPU, timer, keyboard, and display drivers from core kernel logic.
- **Updated Startup Checklist**: Enhanced startup sequence displaying separate `[BOOT]` and `[KERNEL]` system readiness checklists.

### Changed
- **Silenced Serial Timer Spam**: Removed periodic `Ticks: X` serial output while maintaining internal tick accumulation accessible via `phoxtra> ticks`.

## [v0.3.0-alpha] - 2026-07-20

### Added
- **Interrupt Descriptor Table (IDT)**: Created IDT structure, entry mapping, and `idt_load` loader function. Registered all 256 interrupt gates.
- **CPU Exception Handlers**: Implemented CPU exception dispatcher for vectors 0-31 with hex register dump and kernel panic screen.
- **Programmable Interrupt Controller (PIC)**: Added support for remapping PIC1/PIC2 hardware interrupts IRQ 0-15 to vectors `0x20-0x2F`.
- **IRQ System**: Implemented IRQ installation/uninstallation and masking helpers.
- **Timer Driver**: Configured Programmable Interval Timer (PIT) at 100Hz and added system tick counter with periodic logging to serial.
- **Keyboard Driver**: Implemented PS/2 keyboard driver with scancode-to-ASCII conversion, shift state support, and ring buffer.
- **Phoxtra Shell**: Implemented first interactive command shell running on VGA console supporting `help`, `about`, `clear`, `ticks`, and `reboot`.

### Fixed
- **GDT 64-bit Regression**: Resolved triple fault reboot loop by setting proper 64-bit Long Mode code segment descriptor (L=1, D/B=0) and flat data descriptor.
- **Console Scrolling and Backspaces**: Enhanced VGA console driver to scroll up when reaching screen boundary and correctly handle backspaces.

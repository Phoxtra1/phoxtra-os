# Phoxtra OS Architecture Manual

## Overview

Phoxtra OS is a microkernel-inspired, hardware-rooted secure operating system engineered for 64-bit AMD/Intel (x86_64) processors.

---

## Key Subsystem Architecture

### 1. Hardware-Rooted TPM 2.0 SPI Root of Trust
- Hardware attestation via SPI bus.
- SHA-256 process hashing on every PCB execution entry point.
- Immutable Security Event Audit Logger ring buffer.

### 2. Mandatory Access Control (MAC) & Capability Access Control (CAC)
- 64-bit capability bitmasks (`CAP_SYS_ADMIN`, `CAP_DEV_ACCESS`, `CAP_IPC_SEND`).
- Domain isolation (`SYSTEM`, `SERVICE`, `USER`, `RESTRICTED`).
- Hardware entropy-seeded Stack Canary (`__stack_chk_guard`) and 4KB unmapped Kernel Guard Pages.

### 3. Storage Drivers & Real Filesystems
- Block Device Abstraction Layer (`blockdev`).
- AHCI SATA / NVMe Controller Driver (`sata0`).
- MBR & GPT Partition Table Parsers (`sata0p1`, `sata0p2`).
- Ext2 Filesystem (`/boot`, `/usr`) and FAT32 Filesystem (`/home`).

### 4. TCP/IP Network Stack & e1000 Driver
- Intel 82540EM Gigabit Ethernet NIC (`e1000`) with 32-descriptor RX/TX ring buffers.
- Ethernet II, ARP Cache Table, IPv4/ICMP Ping Engine.
- UDP datagrams, TCP Finite State Machine, and BSD Socket API (`socket`, `bind`, `connect`, `listen`, `send`, `recv`).
- Auto-configured DHCP client lease engine and DNS hostname resolver.

### 5. Dynamic Execution Runtime
- ELF64 Executable Binary Loader (`elf_load_binary`).
- Dynamic Linker (`ld.so`) for dynamic shared object (`.so`) resolution.
- Native POSIX C Standard Library (`phoxtra-libc`) implementing `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`.

### 6. Framebuffer & Window Compositor (`phoxtra-wm`)
- VESA VBE / UEFI GOP linear framebuffer (1024x768 @ 32bpp ARGB8888).
- Double-buffered glassmorphic window compositor and 8x16 bitmap font engine.

# Phoxtra OS

Phoxtra OS is a secure, identity-first operating system and the **unified mother foundation powering all Phoxtra physical hardware products**. Rather than existing statically solely for laptops or mobile devices, Phoxtra OS adapts dynamically to empower every class of hardware Phoxtra builds—ranging from automotive safety and telematics systems (**Phoxdrive**) to agricultural soil stations (**FieldSense**), industrial edge gateways, smart mobility units, and enterprise workstations.

The operating system is built around six core principles: **Hardware-Native Empowerment, Security, Identity, Privacy, Trust, and Intelligence**.

---

## 🚀 Current Project Status

Phoxtra OS has successfully evolved through its **v1.0.0 Commercial Release Foundation** and has expanded into its **Universal Hardware Ecosystem (v2.1.0-hardware)**.

### Active Subsystems & Verified Capabilities:
1. **Phoxtra Universal Hardware Device Framework (`kernel/hardware/`)**:
   - Central device registry managing physical Phoxtra hardware profiles: `AUTOMOTIVE`, `AGRICULTURE`, `SMART_MOBILITY`, `INDUSTRIAL_IOT`, `EDGE_GATEWAY`, `MOBILE`, and `DESKTOP_LAPTOP`.
   - Dynamic profile adaptation without relying on fragmented third-party application stacks.
2. **Phoxdrive In-Vehicle Automotive Subsystem (`kernel/hardware/phoxdrive/`)**:
   - **Broken-down Vehicle Radar**: Detection of stationary or stalled vehicles ahead with relative velocity and time-to-impact calculations.
   - **Accident-Prone Zone Geofencing**: Proactive warnings prior to entering high-risk accident hotspots.
   - **Deplorable Road & Pothole Surface Scanner**: High-frequency vertical shock (Z-axis IMU) monitoring evaluating road quality on an index of 0–100.
   - **Speed Telemetry & Synthetic Voice Alert Engine**: Real-time speed cross-checking against road thresholds with audible spoken speech warnings.
3. **FieldSense AgTech Hardware Foundation (`kernel/hardware/fieldsense/`)**:
   - Soil moisture, temperature, pH, ambient humidity telemetry, and automated irrigation solenoid controls.
4. **Core Kernel & Multitasking**:
   - 64-bit Long Mode bootstrap (`boot.S`, Multiboot2).
   - 100 Hz timer-driven preemptive round-robin scheduler with assembly context switching (`context.asm`).
   - Stress tested for up to 64 concurrent tasks.
5. **Memory Subsystem**:
   - 3-tier memory management: Physical page frame allocator (Bitmap PMM), 4-level paging VMM, and dynamic kernel heap (`kmalloc`/`kfree`) with memory leak auditing.
6. **Hardware Root-of-Trust & Zero-Trust Security**:
   - TPM 2.0 SPI MMIO interface with PCR 0–3 measurements.
   - Cryptographic SHA-256 process identity attestation.
   - Immutable hash-chained audit logger.
   - 64-bit Capability Access Control (CAC) and Mandatory Access Control (MAC) domain isolation.
   - Stack canaries, guard pages, and ASLR protection.
7. **Storage, Filesystems & Hardware Discovery**:
   - Hardware Abstraction Layer (HAL) with PCI Express scanner.
   - AHCI SATA controller, MBR/GPT partition parser, ext2 and FAT32 filesystems.
   - Unified Virtual Filesystem (VFS), DevFS, and InitRD.
8. **Networking Stack**:
   - Intel 82540EM Gigabit Ethernet NIC driver.
   - Complete TCP/IP stack: Ethernet II, ARP, IPv4, ICMP Echo, UDP, TCP state machine, BSD Socket API, DHCP client, and DNS resolver.
9. **Display, Window Compositor & Desktop Environment**:
   - High-resolution 1024x768 @ 32bpp linear framebuffer with double buffering.
   - 8x16 anti-aliased bitmap fonts.
   - Window Compositor (`compositor.c`) and Window Manager (`phoxtra-wm`) with glassmorphism aesthetics, dock/taskbar, theme engine, and file manager UI.
10. **Interactive Diagnostic Shell**:
    - Over 35 interactive commands (`help`, `hardware`, `phoxdrive`, `fieldsense`, `ps`, `tpm`, `identity`, `ifconfig`, `ping`, `gui`, `dipa`, `vfs`, etc.).

---

## 📂 Project Structure

```
.
├── boot/                      # Custom Bootloader & UEFI entry stubs
├── config/                    # System and boot configuration profiles
├── docs/                      # Architectural specifications & Roadmaps
│   └── architecture/          # Hardware ecosystem, Phoxdrive specs, memory, security
├── iso/                       # ISO packaging configuration & GRUB bootloader script
├── kernel/                    # Main kernel directory (x86_64 and architecture-independent sources)
│   ├── arch/x86_64/           # x86_64 Assembly boot, IDT/ISR, GDT, context switching
│   ├── hardware/              # Universal Hardware Framework (Phoxdrive, FieldSense, Device Registry)
│   ├── include/               # Core kernel header files
│   ├── log/                   # Kernel serial logging & logger engine
│   ├── memory/                # PMM, VMM, kernel heap, and heap audit
│   ├── net/                   # Intel e1000 NIC, Ethernet, ARP, IPv4, TCP/UDP, sockets
│   ├── process/               # PCB, process manager, multitasking stress engine
│   ├── scheduler/             # Preemptive round-robin scheduler
│   ├── security/              # TPM 2.0, SHA-256 attestation, CAC, MAC, audit logger
│   ├── src/                   # Kernel entry, init, shell, console, interrupts, timer
│   ├── storage/               # AHCI, block devices, ext2, FAT32, partition tables
│   ├── ui/                    # Framebuffer, compositor, window manager (phoxtra-wm), fonts
│   └── vfs/                   # Virtual filesystem, DevFS, InitRD
├── libc/                      # Native C standard library (phoxtra-libc POSIX subset)
├── linker/                    # Custom linker script (`linker.ld`)
├── scripts/                   # Build and release packaging scripts
└── sdk/                       # Developer cross-compiler toolchains and utilities
```

---

## 🛠️ Building & Running Phoxtra OS

### Prerequisites
- **CMake** (v3.20 or newer)
- **GCC / G++** cross-compiler for `x86_64-elf` (or standard GCC with `-ffreestanding`)
- **NASM** (for x86_64 assembly modules)
- **xorriso** & **grub-mkrescue** (for bootable ISO creation)
- **QEMU** (for virtualization and testing)

### Build Commands

1. **Configure and build the kernel**:
   ```bash
   cmake -B build -S .
   cmake --build build
   ```

2. **Generate the bootable ISO**:
   ```bash
   ./scripts/build-iso.sh
   ```
   This generates `phoxtra-os.iso` in the project root.

3. **Run in QEMU**:
   ```bash
   qemu-system-x86_64 -cdrom phoxtra-os.iso -serial stdio
   ```

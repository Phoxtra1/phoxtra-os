# Phoxtra OS

Phoxtra OS is a secure, identity-first operating system designed for desktops, laptops, tablets, mobile devices, servers, and enterprise environments. The operating system is built around five core principles: **Security, Identity, Privacy, Trust, and Intelligence**.

---

## 🚀 Current Project Status

Phoxtra OS is currently in **Phase 1 (v0.1.0-alpha: First successful Phoxtra OS boot)**. The project has a solid foundational bootstrap layer and build setup, but many of the core kernel subsystems and user-space modules are currently placeholders or empty files.

### Done & Fully Functional:
1. **Bootstrapping & Long Mode transition (`kernel/arch/x86_64/boot.S`)**:
   - Implements a Multiboot 2 header.
   - Sets up initial identity paging for the first 16 MB of physical memory using 2 MB huge pages (PML4, PDPT, and PDT).
   - Enables Physical Address Extension (PAE), Long Mode (EFER MSR), and paging.
   - Loads the 64-bit Global Descriptor Table (`gdt64`).
   - Switches to 64-bit Long Mode and jumps to the C++ entry point `kernel_init`.
2. **Serial Logging Driver (`kernel/arch/x86_64/serial/serial.cpp`)**:
   - Basic COM1 serial port logging using inline assembly port I/O (`inb`, `outb`).
   - Outputs initial boot diagnostics `[BOOT]` over serial.
3. **VGA Console Driver (`kernel/src/console.cpp`)**:
   - Simple text-mode VGA console driver mapped to physical address `0xB8000`.
   - Supports `clear()`, `write()`, and custom scrolling/wrapping with standard 80x25 grid representation.
4. **Kernel Entry & Flow (`kernel/src/kernel.cpp`, `kernel/src/init.cpp`)**:
   - `kernel_init` starts the serial port, clears the VGA screen, prints a beautiful "Akwaaba" welcome banner, and starts system-wide initialization.
5. **Build System & Toolchain (`CMakeLists.txt`)**:
   - Complete CMake-based build configuration targeting C++20 and Assembler.
   - Links the kernel binary (`phoxtra`) using a custom linker script (`linker/linker.ld`) at physical address `1M`.
   - Packs the kernel into a bootable ISO using GRUB (`grub-mkrescue`).

### Skeletal / Placeholders (Need Implementation):
- **CPU Management (`kernel/arch/x86_64/cpu/cpu.cpp`)**: Contains a skeletal placeholder for CPU feature/vendor checking.
- **Memory Manager (`kernel/src/memory.cpp` / `kernel/include/memory.hpp`)**: Empty / 0-byte skeleton. Needs physical page allocation (e.g., bitmap or buddy system) and virtual memory mapping.
- **Interrupt Handler (`kernel/src/interrupt.cpp` / `kernel/include/interrupt.hpp`)**: Empty. Needs IDT setup, ISRs, IRQ handling (PIC/APIC), and exception handlers.
- **Scheduler & Processes (`kernel/src/scheduler.cpp`, `kernel/src/process.cpp`)**: Empty stubs. Needs thread control blocks, task queue, and basic context-switching.
- **System Calls & Panic (`kernel/src/syscall.cpp`, `kernel/src/panic.cpp`)**: Skeletal stubs. Needs kernel crash handler (Panic) and syscall dispatcher via `syscall`/`sysret` or interrupts.
- **Bootloader & UEFI (`boot/`)**: Skeletal main files for custom bootloader and UEFI entry.
- **Installer & Recovery (`installer/`, `recovery/`)**: Empty modules.

---

## 📂 Project Structure

```
.
├── boot/                      # Custom Bootloader & UEFI entry stubs
├── config/                    # Empty YAML configurations (system, security, boot, etc.)
├── docs/                      # Technical documentation & Roadmap
│   ├── architecture/          # Engineering principles, memory, and design specifications
│   └── design/                # Design language and visual assets
├── installer/                 # Placeholder for the operating system installer
├── iso/                       # ISO packaging configuration & GRUB bootloader script
├── kernel/                    # Main kernel directory (x86_64 and architecture-independent sources)
│   ├── arch/x86_64/           # x86_64 Assembly boot and hardware-specific drivers
│   ├── include/               # Core kernel header files
│   └── src/                   # Core kernel implementation files (scheduler, memory, etc.)
├── linker/                    # Custom linker script (`linker.ld`)
├── recovery/                  # Recovery mode placeholder
└── scripts/                   # Helper scripts to build and run the ISO
```

---

## 🛠️ Building the Project

### Prerequisites

You need the following tools installed on your host system:
- **CMake** (v3.20 or newer)
- **GCC / G++** cross-compiler for `x86_64-elf` (or standard GCC with `-ffreestanding` support)
- **xorriso** and **grub-common** (for ISO generation with `grub-mkrescue`)
- **QEMU** (for virtualization and testing)

### Build Commands

1. **Configure the project**:
   ```bash
   cmake -B build -S .
   ```

2. **Compile the kernel and bootloader**:
   ```bash
   cmake --build build
   ```

3. **Generate the bootable ISO image**:
   ```bash
   # Using CMake custom target
   cmake --build build --target iso

   # Or using the build script
   ./scripts/build-iso.sh
   ```
   This will generate `phoxtra-os.iso` in the project root (or `build/phoxtra.iso`).

---

## 🖥️ Running Phoxtra OS

You can run the generated ISO image using QEMU:

```bash
qemu-system-x86_64 -cdrom build/phoxtra.iso -serial stdio
```

- `-serial stdio` redirects the kernel's serial port debug output (`[BOOT]`) directly to your host terminal.
- A virtual VGA window will open displaying the graphical console.

---

## 🧭 Next Steps & Roadmap

See [docs/roadmap/roadmap.md](docs/roadmap/roadmap.md) for the detailed, phase-by-phase development plan. The immediate next tasks are:
1. **Interrupt Vector Table (IDT)**: Setup basic hardware interrupts, GDT refresh, and exception handling.
2. **Physical Memory Allocator**: Setup a simple allocator to support dynamic memory allocation in the kernel.
3. **Panic & Diagnostics**: Bring up the panic screen (`kernel_panic`) and assert capabilities to ease debugging.

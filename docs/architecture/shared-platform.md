# Phoxtra Shared Platform Architecture (foundation)

## Purpose
The desktop shell, future Phoxtra hardware products, and Amineva Workspace should consume shared platform services rather than each implementing their own device, storage, identity, and application layers.

## Layer boundaries
1. **Architecture / HAL** — CPU, interrupts, timers, PCI discovery, and hardware drivers. Hardware-specific code must stay behind interfaces.
2. **Kernel core** — memory management, scheduling, IPC/events, syscall boundary, and process lifecycle.
3. **Shared platform services** — input events, display surfaces, VFS/storage, device discovery, identity/capability policy, audit, networking, and application registry.
4. **System applications** — desktop shell, file manager, settings, terminal, installer, and diagnostics. Applications consume services and must not directly manipulate hardware.
5. **Product profiles** — laptop, desktop, and dedicated Phoxtra devices select supported drivers, capabilities, default applications, and UI configuration without forking the platform.
6. **Amineva Workspace** — docs, sheets, slides, and collaboration apps are applications/services layered above the shared platform; this foundation does not claim those apps are implemented.

## Current contracts and gaps
- The existing framebuffer API is the first display backend; it is not yet a general surface/compositor ABI.
- PS/2 keyboard and mouse drivers provide initial input hardware support. A normalized event queue and focus/capture model remain required.
- The VFS API is the storage abstraction; the current in-memory workspace is volatile and is not a persistent filesystem.
- Existing identity, capability, MAC, and audit modules are policy providers. UI labels must not imply hardware-backed TPM, persistent storage, or enforced policy unless the relevant backend confirms it.
- The ELF loader is not yet a complete user-facing application lifecycle/launcher. The app registry must not imply arbitrary applications can launch successfully.

## Service API direction
- **Input service:** device events normalized into pointer, key, button, and focus events; desktop code does not read PS/2 ports.
- **Display service:** framebuffer/surface operations and a future hardware-independent compositor interface.
- **Storage service:** VFS paths and node operations, later backed by persistent block filesystems; callers must handle read-only, missing, and I/O-error cases.
- **Application service:** metadata registry, capabilities, executable format, and explicit launch result/status.
- **Security service:** identity, capabilities, MAC decisions, and auditable allow/deny results; fail closed for protected operations.
- **Device service:** discovered hardware plus supported driver capabilities; no fabricated devices or capacities.

## Delivery gates
- Step 3 is complete only when window actions and file navigation work through the desktop input path and a clean ISO build plus QEMU interaction test passes.
- Step 4 is complete when service ownership, contracts, lifecycle, failure semantics, and product-specific boundaries are reflected in code and documentation.
- Persistent storage, full process launching, and Amineva Workspace applications are separate milestones and must not be represented as finished by these foundations.

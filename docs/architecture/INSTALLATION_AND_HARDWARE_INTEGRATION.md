# Installation and Hardware Integration Foundation

## Purpose

Phoxtra OS is intended to power laptop/desktop computers and future dedicated
Phoxtra electronic devices. Product differences should be expressed through
shared platform services and product profiles, not separate copies of the OS.

## Current implementation reality

- The shared platform API and laptop/desktop/embedded build profiles exist.
- Input, framebuffer display, PCI inventory, security-policy entry points,
  a volatile VFS, and application metadata interfaces exist.
- The current `kernel/storage/ahci.c` implementation is a heap-backed RAM
  simulator. It does not perform physical AHCI controller I/O or enumerate
  real SATA/NVMe media.
- Partition discovery supports primary MBR entries. GPT is detected but not
  enumerated. Partition discovery must not create or write a table.
- The installer is preview scaffolding. Disk selection, partition editing,
  formatting, persistent filesystem installation, bootloader installation,
  and verification are not complete.
- The app registry is metadata; the process and ELF code are not yet a
  complete, safe user-mode application lifecycle. APK/EXE compatibility is
  unsupported until real runtimes exist.

## Required installation flow

1. Detect actual storage devices and report their model, capacity, sector size,
   capabilities, and read/write errors.
2. Read partition metadata without changing the device. Support MBR and GPT
   enumeration before exposing both to the installer.
3. Let the user select a target device and clearly show existing partitions
   and planned changes.
4. Build a dry-run operation plan and require explicit confirmation before
   any destructive operation. Never repartition a disk on ordinary boot.
5. Apply only validated operations; check device bounds, filesystem/type
   support, and every I/O result.
6. Format supported target partitions, copy system files, create boot
   configuration, install the boot path, and verify the result.
7. Stop safely and report the exact failed stage on any unsupported device,
   read/write error, or verification failure.

## Required software-to-hardware integration path

Applications and system services should call the shared platform API. The
platform API resolves operations through the configured product profile and
available device capabilities. Drivers own controller/register/DMA details;
the HAL and platform services own stable interfaces; the UI and applications
must not perform raw hardware access.

The application lifecycle must provide a real package format, signature
verification, bounded installation, executable validation, user-mode process
creation, memory isolation, syscall access, capability checks, application
launch, and explicit failure/exit reporting. Planned apps must remain marked
as planned until these functions are implemented and tested.

## Implementation order

1. Keep boot-time partition discovery read-only and label the current storage
   backend honestly as a simulator.
2. Implement and test a real persistent block-device driver.
3. Add MBR/GPT partition enumeration and persistent filesystem mounting.
4. Implement a guarded installer with preview, explicit disk selection,
   destructive-operation confirmation, format/copy/boot setup, and verification.
5. Complete user-mode application loading and lifecycle APIs.
6. Add a signed Phoxtra package format and installer, then integrate applications
   through the same shared service contracts.
7. Validate hardware capabilities and product profiles on QEMU and physical
   hardware separately.

## Acceptance gates

A build or desktop boot alone is not proof of persistent installation or app
compatibility. Installation is ready only after storage tests, MBR/GPT test
images, negative/invalid-table tests, write-protection tests, install/reboot
tests, and data-preservation checks pass. Application support is ready only
after package validation, signature rejection, launch/exit, memory isolation,
capability denial, and uninstall/rollback tests pass.

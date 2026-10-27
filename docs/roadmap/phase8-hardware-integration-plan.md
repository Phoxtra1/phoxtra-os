# Phase 8 — First Dedicated Phoxtra Hardware Integration Plan

**Status: IN PROGRESS — software readiness and integration preparation.** Target-specific dedicated-board bring-up remains gated on Phase 7 physical-laptop acceptance and selection of an actual Phoxtra hardware target. This work does not mark Phase 7 passed.

## 1. Current platform reality

- Boot target currently validated by CI is x86_64 under QEMU using a GRUB/Multiboot2 ISO.
- The prototype has a 32 MiB volatile RAM test device plus experimental read-only PCI AHCI/SATA access for supported ATA LBA48 disks with 512-byte logical sectors. AHCI/SATA has passed automated QEMU read tests; no physical laptop controller has been validated. NVMe remains unsupported.
- MBR partition discovery and a limited read-only on-disk ext2 subset are implemented and exercised by CI. Journaled/indexed/unsupported ext2 features are rejected. On-disk FAT32, persistent writes, flush semantics, filesystem repair, and write-based recovery are not implemented.
- The installer and partition formatting path are disabled. Partition discovery must never initialize, rewrite, or format a disk.
- No dedicated Phoxtra board, CPU, firmware, display, power, thermal, or peripheral specification is approved in this plan yet.

## 2. Phase 8 software readiness work

The Phase 8 software-preparation slice adds a read-only hardware readiness report, available in the shell as `hardware`, and emits a startup inventory marker for CI. It reports the x86_64/Multiboot2 target, product profile, display and input availability, PCI device classes, simulated block-device count, and whether a non-simulated block-device driver or persistent filesystem is registered.

The report is observational only. PCI class detection is not proof that a matching driver works. Read-only AHCI/SATA and the supported ext2 reader have QEMU/CI evidence but not physical-laptop acceptance; the RAM disk remains simulated and volatile. No path in this prototype should format, partition, or write to an internal physical disk.

## 3. Phase 7 prerequisite gate

The user may defer physical testing until their schedule allows; repository preparation can continue meanwhile. Do not declare Phase 7 passed or begin target-specific dedicated-board driver work until the standard-laptop checklist is recorded as passed, including boot/reboot, display, keyboard, pointer/touchpad functionality, desktop interaction, stability, and accurate storage-device reporting. CI/QEMU results alone are not enough.

## 4. Target definition required before bring-up

When Phase 7 passes, create a reviewed target specification that records:

- Board and revision, CPU architecture and features, RAM, firmware/boot mode, and recovery mechanism.
- Display controller and supported modes.
- Keyboard, pointer/touch input and required buses.
- Storage controller/device type and a non-destructive detection/read-test plan.
- Power input, battery/charging interface if applicable, thermal sensors, and safe shutdown behavior.
- Network, USB, audio, and other required peripherals.
- A documented way to recover or reflash the device without depending on the Phoxtra OS booting.

Do not guess these specifications before a real target board is selected.

## 5. Bring-up sequence

1. Verify firmware boot and serial/diagnostic output.
2. Confirm CPU features and memory map without modifying persistent storage.
3. Bring up display and keyboard/input.
4. Detect storage read-only; do not write partition tables or format devices.
5. Validate power, thermals, reset, and shutdown behavior.
6. Add only the drivers required by the chosen board, each in an isolated change.
7. Re-run kernel builds, ISO/profile checks where applicable, automated boot tests, and target-specific tests.
8. Record failures and retest before advancing.

## 6. Acceptance evidence

Phase 8 can be marked passed only after the selected dedicated device boots reliably and the required display, input, storage safety, power/thermal, recovery, and stability tests have recorded evidence. Until then, its status remains blocked/not started.

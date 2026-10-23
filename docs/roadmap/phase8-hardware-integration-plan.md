# Phase 8 — First Dedicated Phoxtra Hardware Integration Plan

**Status: BLOCKED pending Phase 7 physical-laptop acceptance.** This document is preparation only. It does not authorize hardware integration or mark the laptop gate as passed.

## 1. Current platform reality

- Boot target currently validated by CI is x86_64 under QEMU using a GRUB/Multiboot2 ISO.
- The current block-device backend is a 32 MiB volatile RAM test device, not a physical AHCI or NVMe implementation.
- Current Ext2/FAT32 adapters expose demo in-memory content; they do not provide real on-disk filesystem support.
- The installer and partition formatting path are disabled. Partition discovery must never initialize, rewrite, or format a disk.
- No dedicated Phoxtra board, CPU, firmware, display, power, thermal, or peripheral specification is approved in this plan yet.

## 2. Phase 7 prerequisite gate

Do not begin target-specific driver work until the standard-laptop checklist is recorded as passed, including boot/reboot, display, keyboard, pointer/touchpad functionality, desktop interaction, stability, and accurate storage-device reporting. CI/QEMU results alone are not enough.

## 3. Target definition required before bring-up

When Phase 7 passes, create a reviewed target specification that records:

- Board and revision, CPU architecture and features, RAM, firmware/boot mode, and recovery mechanism.
- Display controller and supported modes.
- Keyboard, pointer/touch input and required buses.
- Storage controller/device type and a non-destructive detection/read-test plan.
- Power input, battery/charging interface if applicable, thermal sensors, and safe shutdown behavior.
- Network, USB, audio, and other required peripherals.
- A documented way to recover or reflash the device without depending on the Phoxtra OS booting.

Do not guess these specifications before a real target board is selected.

## 4. Bring-up sequence

1. Verify firmware boot and serial/diagnostic output.
2. Confirm CPU features and memory map without modifying persistent storage.
3. Bring up display and keyboard/input.
4. Detect storage read-only; do not write partition tables or format devices.
5. Validate power, thermals, reset, and shutdown behavior.
6. Add only the drivers required by the chosen board, each in an isolated change.
7. Re-run kernel builds, ISO/profile checks where applicable, automated boot tests, and target-specific tests.
8. Record failures and retest before advancing.

## 5. Acceptance evidence

Phase 8 can be marked passed only after the selected dedicated device boots reliably and the required display, input, storage safety, power/thermal, recovery, and stability tests have recorded evidence. Until then, its status remains blocked/not started.

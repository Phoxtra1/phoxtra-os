# Phoxtra OS

**Phoxtra OS is an experimental x86_64 operating-system prototype under active development.** Its current bootable ISO brings up a kernel, a graphical desktop environment, keyboard and pointer interaction in the tested virtual machine, a small set of built-in system panels, and an application catalogue. Automated CI builds the ISO and exercises core desktop interactions in QEMU.

The long-term Phoxtra direction includes software integrated with Phoxtra devices. This repository currently validates an x86_64 prototype; it is **not** a production-ready general-purpose operating system, and this ISO is **not** a mobile-phone build.

## Current state

**Prototype stage — automated virtual-machine checks pass; physical-laptop validation is still pending.**

The current CI pipeline:
- Builds the default x86_64 kernel and ISO.
- Builds the desktop and embedded product profiles.
- Checks storage-write safety and the hardware-readiness probe.
- Boots the ISO under QEMU and verifies framebuffer/console initialization, desktop event-loop startup, themes, Application Center/Amineva Workspace catalogue access, functional mouse/file-manager interactions, RAM-backed file and folder creation, and screenshot rendering.
- Produces an ISO plus a SHA-256 checksum as a downloadable workflow artifact.

A successful CI run proves that those automated checks passed for the tested build. It does **not** prove that the ISO boots correctly on every physical laptop or that the OS is complete.

## Implemented in the prototype

- x86_64 bootable ISO using GRUB/Multiboot2.
- Graphical framebuffer desktop and window/compositor services.
- Keyboard input and a PS/2 mouse path used by the QEMU interaction tests.
- Built-in Shell Console, Files, and System Status catalogue panels.
- Theme switching, Application Center, and a read-only `hardware` diagnostic report.
- A shared platform service interface and a catalogue that distinguishes available native panels from planned integrations.
- Storage-safety controls that keep partition discovery read-only, identify the RAM disk as simulated, and leave disk installation/formatting disabled.
- Automated build, product-profile, QEMU, rendering, and storage/hardware-readiness checks.

## Important limitations

These are known scope limitations, not features to assume are present:

- **No persistent physical-disk storage is connected.** The current block device is a volatile, read-only 32 MiB RAM test disk. Files created in the RAM-backed environment are lost at reboot.
- The Ext2/FAT32 components are demo in-memory VFS adapters, not complete on-disk filesystem drivers.
- **The installer is disabled.** Do not install or format a laptop's internal drive with this prototype.
- Amineva Global products and Amnewsworld entries in the Application Center are catalogue entries. Only the listed native panels are currently marked available; external apps are not yet launchable from Phoxtra.
- **Amineva Workspace is not yet integrated as a functioning suite.** Docs, Sheets, Slides, Forms, Drive, account sign-in, cross-application file access, and offline operation must not be considered available from the OS until their real service/runtime connections are implemented and tested.
- A general-purpose browser and fully validated end-user network/TLS/web-app path are not delivered as a complete user feature.
- A physical standard-laptop test has not yet passed the Phase 7 acceptance checklist.
- No dedicated Phoxtra hardware target has been selected and brought up.
- There is no validated ARM64 mobile-phone build. Mobile support remains a future project, not a capability of this ISO.
- Security, identity, driver coverage, persistence, recovery, and app/runtime capabilities need more engineering before production use.

## Build and test

The project is built on Ubuntu in GitHub Actions. For local development, install the project's build dependencies (CMake, build tools, NASM, GRUB/ISO tools, QEMU, socat, and Python 3), then run:

```bash
python3 scripts/check_storage_safety.py
cmake -B build -S .
cmake --build build -j2
cmake --build build --target iso
```

The generated ISO is typically at `build/phoxtra.iso`. To start it in QEMU:

```bash
qemu-system-x86_64 -m 256M -cdrom build/phoxtra.iso -serial stdio
```

Follow the current workflow in [`.github/workflows/phoxtra-build.yml`](.github/workflows/phoxtra-build.yml) for the canonical CI test sequence. A local build by itself is not a release sign-off.

## Prototype completion and release policy

Use these documents as the source of truth for status and next actions:

- [Prototype completion and release gates](docs/release/prototype-completion-and-release-gates.md)
- [Amineva Workspace integration plan](docs/integration/amineva-workspace-integration-plan.md)
- [Funding and prototype brief](docs/funding/prototype-brief.md)
- [Standard-laptop validation checklist](docs/roadmap/laptop-validation-checklist.md)
- [Phase 8 hardware integration plan](docs/roadmap/phase8-hardware-integration-plan.md)
- [Development roadmap](docs/roadmap/roadmap.md)

The release workflow supports creating an **unpublished draft GitHub Release** from a successful manual build. It is opt-in and does not publish a public release automatically. Use it only after release checks are reviewed. Build artifacts expire; a draft/published Release is the versioned distribution location. Do not publish the release until the physical-laptop acceptance gate, known-limitations review, and release notes are complete.

## Funding and demonstration

Phoxtra OS can be described honestly as an early-stage, bootable desktop OS prototype with automated QEMU evidence and a staged hardware-readiness plan. Suitable opportunities may include prototype-development grants, R&D support, incubators, university or maker-lab programmes, and hardware/operating-system innovation support. Eligibility and fit depend on each funder's criteria; no funding outcome is guaranteed.

Do not claim production readiness, persistent disk support, finished Amineva Workspace integration, mobile-phone support, or successful physical hardware validation until the corresponding work has been implemented and evidenced.

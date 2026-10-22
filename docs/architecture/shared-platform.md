# Phoxtra Shared Platform Architecture — Step 4

## Goal

Phoxtra OS is intended to power laptop/desktop systems and future dedicated electronic products from one shared platform. Product differences should be configuration and capability choices, not separate copies of the OS. Amineva Workspace is planned as a set of applications on top of these services, not a kernel subsystem.

## Layer ownership

1. **Architecture and HAL:** CPU, interrupt controllers, timers, PCI enumeration, framebuffer and keyboard/mouse drivers. Hardware register access stays in these drivers.
2. **Kernel core:** memory allocation, scheduling/process structures, syscall boundary, event infrastructure and VFS core.
3. **Shared platform services (`kernel/services/platform.h/.c`):** normalized input events, display information and operations, storage access, device inventory, application metadata, security-policy entry points and product profile.
4. **System UI and applications:** desktop window manager, file manager, terminal, settings and installer. UI code uses shared input/storage contracts where integrated; UI owns layout and interaction, not hardware ports or storage internals.
5. **Product profile:** `PHOXTRA_PRODUCT_PROFILE=laptop|desktop|embedded` chooses the profile identifier and product expectations at build time. The same kernel and services are retained.
6. **Amineva Workspace:** future Docs, Sheets, Slides and collaboration applications registered as planned applications until a real user-mode runtime and launch lifecycle can run them.

## Implemented service contracts

### Input
`platform_input_poll` normalizes keyboard characters and pointer movement/button events into one event structure. The desktop window manager consumes this interface rather than polling PS/2 drivers itself. The current driver backend is keyboard plus PS/2 mouse when detected; touch, USB HID and hot-plug support are not claimed.

### Display
`platform_display_get_info` reports the framebuffer's actual dimensions, pitch, bpp and availability. Drawing and present wrappers currently route to the framebuffer backend. This is an abstraction boundary, not a GPU acceleration API or a complete user-space surface ABI. The existing compositor remains the desktop compositor.

### Storage
`platform_storage_* ` wraps VFS lookup, listing, reading, writing and creation. The current VFS workspace is volatile RAM; `platform_storage_get_info` explicitly reports that data is lost on reboot. Persistent SATA/AHCI, NVMe, Ext2/FAT32 mounting and the file manager must not be described as production-ready just because driver modules exist.

### Device inventory
`platform_device_* ` reports enumerated PCI device IDs/classes from the existing PCI scanner. It does not claim that every discovered device has a working driver or is supported by the current product.

### Application registry
The registry describes the built-in terminal and file manager as currently available, and Settings, App Center and Amineva Workspace as planned. Metadata discovery is not application launching, package installation or process isolation. A future launcher must return explicit launch failures and must not label a planned application as installed/running.

### Security
`platform_security_check_vfs` and `platform_security_has_capability` provide a common entry point to existing MAC/capability providers. Protected checks deny missing caller identity or missing node context. The kernel's current internal desktop VFS helpers run in kernel context; they are not a substitute for enforcing policy at a future user/kernel syscall boundary. TPM, audit persistence and capability enforcement require separate backend verification.

## Error and lifecycle rules

- Public service calls validate null pointers and invalid indexes and return explicit result codes where applicable.
- Unsupported hardware is reported as unavailable/unsupported, not fabricated.
- Storage calls may return zero bytes or a failure code when the path/node is missing or not of the expected type.
- Service initialization happens after the driver and VFS/security providers are initialized. Status flags describe detected/initialized services, not feature-completeness certification.
- Product-specific features should be gated by detected device capabilities and selected profile; the profile is an expectation/default, not proof that the hardware is present.

## QEMU validation

After the Step 4 PR is merged, configure/build the ISO and boot it in QEMU. Run `platform` at the shell and confirm profile, actual framebuffer mode, keyboard/pointer status, volatile storage disclaimer, PCI count and app registry. Then enter `gui` and test window dragging/close/minimize/maximize, folder navigation, folder/file creation and preview. Run the build's CI/QEMU test as a separate check.

## Deliberate non-goals for Step 4

This step does not implement persistent disk storage, a complete process launcher, GPU acceleration, animations, USB/touch drivers, network readiness, signed package management, or Amineva Docs/Sheets/Slides themselves. Those require dedicated later milestones and hardware/runtime validation.

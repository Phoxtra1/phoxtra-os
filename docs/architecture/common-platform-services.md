# Step 5 — Common Platform Services

## Purpose

Desktop UI and future Phoxtra laptop, desktop, and embedded devices share one
platform contract. Product-specific behavior belongs in the selected product
profile or a service backend, not in duplicated desktop implementations.

## Shared service boundaries

- **Input:** normalized keyboard and pointer events; UI code must not access
  PS/2 ports directly.
- **Display:** dimensions, pixel/rectangle drawing, clear, and present operations
  are exposed through the platform display boundary. The current backend is a
  framebuffer; this does not imply GPU acceleration.
- **Storage:** path lookup, directory listing, file reads/writes, and RAM-backed
  creation are exposed through the platform storage boundary. The current VFS
  is volatile RAM storage, and contents are lost on reboot.
- **Hardware inventory:** device records come from PCI enumeration and should
  not be invented when hardware is absent.
- **Application registry:** metadata and planned/available states describe
  discoverability only. They do not claim a process launcher or application
  sandbox is complete.
- **Security:** caller-aware capability and mandatory-access-control checks
  fail closed when caller identity is absent.
- **Product profile:** laptop, desktop, and embedded defaults select policy
  without forking shared services.

## Initialization order

Initialize hardware and input drivers first, then memory and PCI, security
providers, framebuffer/UI, VFS, and finally the shared platform services after
their dependencies are available. Consumers should query service capabilities
rather than assuming hardware exists.

## Current status and honest limits

Implemented foundation: service APIs, status flags, product profiles, normalized
input, framebuffer boundary, volatile VFS operations, PCI inventory, application
metadata, and security-policy entry points.

Not yet production-complete: persistent block storage and filesystem mounting,
hot-plug/device lifecycle management, full user-mode process launch, signed
application installation, and the Amineva Workspace Docs/Sheets/Slides apps.
Those must be added behind these service boundaries rather than by duplicating
desktop-specific code.

## Step 5 completion gate

A clean kernel/ISO build, QEMU boot, initialized font/service diagnostics,
desktop rendering, and file/folder operations must pass CI. Mouse and keyboard
interaction, window controls, and actual hardware behavior still require the
manual laptop milestone in Step 6.

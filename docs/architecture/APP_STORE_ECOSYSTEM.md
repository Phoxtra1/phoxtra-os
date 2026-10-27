# Phoxtra OS Application Ecosystem — Target Design and Status

This document describes the intended software ecosystem. It is not a claim
that every listed compatibility layer, cloud service, package format, or
security integration is currently implemented.

## Foundation currently present

- Shared platform service APIs for input, display, storage access, PCI
  inventory, product profiles, policy hooks, and application metadata.
- An application registry, where planned entries are metadata and do not
  imply that an app can be installed or launched.
- Early process/scheduler structures and an ELF64 loader prototype.
- A RAM-backed VFS workspace and a simulated RAM block device.

## Target application layers

| Layer | Target | Current implementation status |
|---|---|---|
| Native Phoxtra apps | Signed `.pxa` packages and a documented app manifest | Package format, verifier, installer, and launcher are not complete |
| Linux desktop software | Selected Flatpak/AppImage or compatible native applications | Runtime, POSIX coverage, sandbox, and package integration are not implemented |
| Web applications | PWAs and WebAssembly applications | Browser/WebView runtime and app sandbox are not implemented |
| Command-line software | Native ELF64 applications using a stable syscall/API contract | A safe user-mode application lifecycle is not complete |
| Android/Windows compatibility | APK/EXE support through isolated compatibility runtimes | `phoxtra_box` is a placeholder; APK/EXE execution is unsupported |

## Required application lifecycle

Before an application is advertised as installable or runnable, Phoxtra needs:

1. A versioned package manifest containing application ID, version,
   architecture, entry point, required platform version, and requested
   capabilities.
2. Package integrity and publisher-signature verification before installation.
3. A safe installation transaction with path validation, rollback on failure,
   and explicit result reporting.
4. A real user-mode loading and launch path with validated executable ranges,
   mapped address spaces, a syscall ABI, process identity, permissions, and
   exit/status reporting.
5. Capability checks for storage, network, input, display, and device access.
6. Compatibility tests for each declared hardware/product profile.

The registry must continue to mark applications as planned until their
installation and launch lifecycle has passed tests. An API, log message, or
registry entry alone is not proof of working software support.

## Security and cloud services

Package signing, a Phoxtra Store service, PhoxtraID trust integration, and
hardware-rooted verification are future integration requirements. Their
presence in the architecture must not be treated as proof that a production
service, certificate chain, TPM binding, or secure package pipeline is ready.

## Integration rule

Applications must use the shared Phoxtra platform contracts rather than
accessing hardware ports directly. Hardware-dependent capabilities must be
discovered and checked at runtime. Laptop, desktop, and embedded profiles
select product policy without forking the shared service interfaces.

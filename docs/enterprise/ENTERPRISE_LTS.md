# Phoxtra OS Enterprise LTS & Support SLA

Welcome to the **Phoxtra OS Enterprise LTS Maintenance Guide**. This document details our 5-year Long Term Support (LTS) policy, security patch pipeline, and bare-metal hardware certification standards for commercial enterprise deployments.

---

## 1. Enterprise Long Term Support (LTS) Lifecycle

Phoxtra OS v1.0.0 is designated as an **Enterprise LTS Branch** backed by guaranteed 5-year support (2026–2031).

- **Core Kernel Bugfixes**: Quarterly rollup releases containing hardware compatibility patches and memory subsystem optimizations.
- **Security Hotpatching**: 24-hour response SLA for high-severity CVE vulnerabilities.
- **API/ABI Stability Guarantee**: System call vectors (Vectors `0x00`–`0x3F`) and `phoxtra-libc` standard headers are ABI-locked for all v1.0.x versions.

---

## 2. Security Patch Pipeline & CVE Reporting

Security vulnerabilities are tracked by the Phoxtra Security Response Team (PSRT).

### Security Architecture Protections
- **TPM 2.0 SPI Root of Trust**: Hardware attestation validating kernel load image checksums on cold boot.
- **Capability Access Control (CAC)**: Explicit 64-bit capability bitmasks required for sensitive hardware access.
- **Mandatory Access Control (MAC)**: Process security domains (`SYSTEM`, `SERVICE`, `USER`, `RESTRICTED`).
- **Stack Canary & Unmapped Guard Pages**: Hardware entropy-seeded stack protection preventing buffer overflow exploits.

To report security issues, email `security@phoxtra-os.org` with GPG-encrypted details.

---

## 3. Bare-Metal Hardware Certification

Phoxtra OS v1.0.0 is certified across modern commercial desktop and server hardware platforms:

- **CPUs**: Intel Core 8th Gen to 14th Gen, Intel Xeon Scalable, AMD Ryzen 3000-7000 Series, AMD EPYC.
- **Graphics**: VESA VBE 2.0/3.0 & UEFI GOP linear framebuffers (1024x768, 1920x1080 @ 32bpp).
- **Network**: Intel 82540EM, Intel I210/I350 Gigabit, VirtIO-Net, Realtek RTL8111/8168.
- **Storage**: AHCI SATA controllers, NVMe PCI Express SSDs, MBR and GPT partition tables.

#!/usr/bin/env python3
"""Guard against prototype stubs reporting security/runtime success."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
def read(path): return (root / path).read_text(encoding="utf-8")
checks = {
    "production release": ("kernel/release/production.c", ["v1.0.0 Commercial Release", "Security SLA Active", "CERTIFIED"]),
    "fake TPM": ("kernel/security/tpm2.c", ["hardware_attested       = 1", "ENDORSEMENT_KEY_SEED", "hardware_attested = 1"]),
    "fake verification": ("kernel/security/verify.c", ["return 1; // Authentic", "Blockchain Ledger", "Root CA"]),
    "fake dynamic linker": ("kernel/exec/ld_so.c", ["0x7FFFF7A00000ULL", "0x7FFFF7A01050ULL", "symbol_count = 128"]),
    "fake APK/EXE runtime": ("kernel/exec/phoxtra_box.c", ["return 1;", "Executing APK", "Executing EXE"]),
    "fake user mode claim": ("kernel/vfs/initrd.c", ["RING 3 ENABLED", "security.isolation=strict"]),
}
errors = []
for label, (path, needles) in checks.items():
    body = read(path)
    for needle in needles:
        if needle in body:
            errors.append(f"{label} still contains unsupported-success marker: {needle}")
# Shell diagnostics must not turn missing implementations into positive status reports.
shell = read("kernel/src/shell.c")
for marker in (
    "COMMERCIAL PRODUCTION LAUNCH", "0% packet loss", "100% Genuine & Authentic",
    "3D Vector Lock", "All Enterprise Components Unlocked", "RING 3 Userland Isolation",
    "Immutable Kernel Security Event Audit Log", "Phoxtra Store App Index & Signatures",
    "Hardware TPM 2.0, CAC/MAC Security, TCP/IP, VBE GUI, ELF64 Runtime, SDK"
):
    if marker in shell:
        errors.append(f"shell still contains unsupported success claim: {marker}")
for path, marker in (
    ("kernel/services/entitlements.c", "return 1;"),
    ("kernel/ai/dipa_core.c", "dipa_active = 1"),
    ("kernel/services/phoxair.c", "active_targets = 4"),
    ("kernel/sdk/sdk.c", "return 64;"),
    ("kernel/security/process_identity.c", "authority_secret_key"),
    ("kernel/memory/heap_audit.c", "0 orphaned pointers, 0 memory leaks detected"),
    ("kernel/process/stress.c", "All tasks verified under timer preemption")
):
    if marker in read(path):
        errors.append(f"{path} still contains unsupported-success marker: {marker}")
guard = read("kernel/security/stack_guard.c")
if "vmm_unmap_page(guard_page_addr)" in guard or "Guard Page mapped" in guard:
    errors.append("stack guard must not unmap an arbitrary stack page or claim a guard page exists")
if "return -2;" not in read("kernel/exec/elf.c"):
    errors.append("ELF execution must fail closed until process address-space loading exists")
if errors:
    print("Prototype claim-integrity audit FAILED:")
    for error in errors: print(" - " + error)
    sys.exit(1)
print("Prototype claim-integrity audit passed: unsupported security, release, linker, runtime and user-mode features do not report success.")

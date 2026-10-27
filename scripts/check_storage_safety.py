#!/usr/bin/env python3
"""Fail the build if the prototype reintroduces automatic disk writes."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]

def read(path):
    return (root / path).read_text(encoding="utf-8")

errors = []

# Guard boundary handling and reject malformed/overlapping partition declarations.
partitions = read("kernel/storage/partition.c")
blockdev = read("kernel/storage/blockdev.c")
blockdev_header = read("kernel/storage/blockdev.h")
journal = read("kernel/storage/storage_journal.c")
journal_test = read("tests/test_storage_journal.c")
ramdisk = read("kernel/storage/ahci.c")
ext2 = read("kernel/storage/ext2.c")
fat32 = read("kernel/storage/fat32.c")
ahci = read("kernel/storage/ahci.c")
workflow = read(".github/workflows/phoxtra-build.yml")
if "lba >= part->sector_count" not in partitions or "count == 0" not in partitions:
    errors.append("partition reads must reject zero-length and out-of-range requests")
if "ranges_overlap" not in partitions or "MBR_BOOT_ACTIVE" not in partitions:
    errors.append("partition parser must validate overlap and MBR boot-indicator fields")
if "is_extended_partition" not in partitions or "extended partition skipped" not in partitions:
    errors.append("unsupported MBR extended-partition chains must not be exposed as ordinary filesystems")
if "blockdev_register" not in blockdev or "duplicate pointer or name" not in blockdev:
    errors.append("block-device registry must reject duplicate registrations")

# Persistent writes are a gated milestone: only registered, explicitly enabled,
# simulated, non-read-only devices with a successful durability hook are eligible.
if "BLOCKDEV_FLAG_WRITE_ENABLED" not in blockdev_header or "flush_sectors" not in blockdev_header:
    errors.append("block-device API must expose explicit write enablement and a flush hook")
for required in ("BLOCKDEV_FLAG_SIMULATED", "BLOCKDEV_FLAG_WRITE_ENABLED",
                 "BLOCKDEV_FLAG_READONLY", "blockdev_is_registered",
                 "dev->write_sectors(dev, lba, count, buffer)",
                 "blockdev_flush(dev) == 0"):
    if required not in blockdev:
        errors.append(f"guarded block writes are missing required safety gate: {required}")
if "part->block_dev.flags = BLOCKDEV_FLAG_PARTITION | BLOCKDEV_FLAG_READONLY" not in partitions:
    errors.append("discovered partitions must remain read-only and must not inherit write enablement")
for required in ("JOURNAL_MAGIC", "crc32", "JOURNAL_CRC_OFFSET", "recover_locked",
                 "target_is_allowed", "write_done_pair", "JOURNAL_STATE_DONE",
                 "replay_commit", "blockdev_write_sectors",
                 "journal->journal_start_lba + 1u", "journal->journal_start_lba + 2u"):
    if required not in journal:
        errors.append(f"safe storage journal is missing required recovery invariant: {required}")
for required in ("fail_at <= 6u", "partial_write_bytes", "fail_flush_call",
                 "conflicting metadata copies fail closed",
                 "corrupt payload is never written to target",
                 "read-only device refuses journal initialization",
                 "pre-commit interruption preserves old target",
                 "committed interruption replays new target"):
    if required not in journal_test:
        errors.append(f"storage journal regression tests missing failure case: {required}")
if "dev != &ram_test_dev" not in ramdisk or "dev->total_sectors - lba" not in ramdisk:
    errors.append("RAM test device reads must be limited to the registered simulated device and advertised geometry")
# Check the write callbacks that actually exist in the single current RAM-test backend.
# Do not require a second, obsolete ram_test_write_sectors implementation.
for write_fn in ("ram_test_write_sectors", "ahci_write_sectors"):
    start = ramdisk.find("int " + write_fn + "(")
    if start < 0:
        continue
    end = ramdisk.find("\n}", start)
    body = ramdisk[start:end] if end > start else ""
    if not body or "return -1;" not in body:
        errors.append(f"{write_fn} must fail closed; storage writes are not implemented")
for label, source in (("Ext2", ext2), ("FAT32", fat32)):
    if "file->size - offset" not in source:
        errors.append(f"{label} demo reads must avoid offset-plus-size overflow")
    if "if (!file)" not in source:
        errors.append(f"{label} demo mount must handle metadata allocation failure")
partition = read("kernel/storage/partition.c")
parse = re.search(r"int partition_parse\(block_device_t\* master\)\s*\{(.*?)\n\}", partition, re.S)
if not parse:
    errors.append("partition_parse() could not be located")
else:
    body = parse.group(1)
    if re.search(r"write_sectors|partition_write_sectors|create_partition_subdev\s*\(\s*master\s*,\s*\"sata", body):
        errors.append("partition_parse() must never write sectors or invent partition devices")
    if "device left unchanged" not in body or "valid MBR signature" not in body:
        errors.append("invalid/missing partition tables must be rejected without changes")

if "BLOCKDEV_FLAG_SIMULATED" not in read("kernel/storage/blockdev.h"):
    errors.append("block device flags must identify simulated storage")
# This prototype intentionally has a volatile RAM test disk, not a physical AHCI driver.
# Require truthful readiness labeling and read-only behavior; don't demand hardware paths
# that do not exist and must not be advertised as implemented.
for required in ("ahci_read_sectors", "ahci_write_sectors", "RAM test disk",
                 "BLOCKDEV_FLAG_SIMULATED", "BLOCKDEV_FLAG_READONLY",
                 "physical AHCI/NVMe I/O is not implemented"):
    if required not in ahci:
        errors.append(f"RAM storage backend missing read-only safety marker/path: {required}")
if "dev != &ram_test_dev" not in ahci or "lba >= dev->total_sectors" not in ahci or "dev->total_sectors - lba" not in ahci:
    errors.append("RAM test disk reads must be restricted to the registered device and bounded by geometry")
ram_backend = ahci.split("/* Simulated volatile RAM storage", 1)[-1].split("\nint ahci_read_sectors", 1)[0]
if re.search(r"AHCI controller|PCI.*AHCI|HBA", ram_backend, re.I):
    errors.append("RAM test backend must not claim to implement physical AHCI")

# Physical ext2 mounts must require read-only partitions and expose no write callback.
for required in ("ext2_mount_readonly", "EXT2_SUPER_MAGIC", "ext2_get_data_block",
                 "BLOCKDEV_FLAG_READONLY", "BLOCKDEV_FLAG_PARTITION",
                 "EXT2_FEATURE_COMPAT_HAS_JOURNAL", "EXT2_INDEX_FL", "EXT2_VALID_FS",
                 "filesystem state is unclean or marked with errors"):
    if required not in ext2:
        errors.append(f"read-only ext2 implementation missing validation: {required}")
if "node->write =" in ext2 or "ext2_file_write" in ext2:
    errors.append("ext2 VFS nodes must not expose a write callback in the read-only prototype")
for required in ("e2fsprogs", "mke2fs", "debugfs", "snapshot=on",
                 "Ext2: mounted supported on-disk filesystem read-only; writes and indexed directories are disabled",
                 "Ext2: confirmed read-only file data access",
                 "sha256sum -c /tmp/phoxtra-ahci-test.before.sha256"):
    if required not in workflow:
        errors.append(f"QEMU storage integration regression coverage missing: {required}")

installer = read("kernel/installer/installer.c")
if "Installer disabled" not in installer or "compositor_create_window" in installer:
    errors.append("installer must fail closed and must not launch a pretend install wizard")

shell = read("kernel/src/shell.c")
install_fn = re.search(r"static void cmd_installer\(void\)\s*\{(.*?)\n\}", shell, re.S)
if not install_fn or "not available" not in install_fn.group(1) or "installer_start" in install_fn.group(1):
    errors.append("shell installer command must report unavailable and perform no installation")

if "services/hardware_probe.c" not in read("kernel/CMakeLists.txt"):
    errors.append("hardware readiness probe must be compiled into the kernel")
if "phoxtra_hardware_probe_log();" not in read("kernel/src/init.cpp"):
    errors.append("hardware readiness probe must run after device initialization")
if "PHOXTRA_HWPROBE_READY" not in read("kernel/services/hardware_probe.c"):
    errors.append("hardware readiness probe must expose its CI marker")
if "PHOXTRA_HWPROBE_READY" not in read(".github/workflows/phoxtra-build.yml"):
    errors.append("QEMU CI must verify the hardware readiness probe")
if 'kstrcmp(input, "hardware") == 0' not in read("kernel/src/shell.c"):
    errors.append("read-only hardware report must be accessible through the shell")

if errors:
    print("Storage and hardware readiness audit FAILED:")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Storage and hardware readiness audit passed: bounded read-only AHCI/SATA I/O, read-only ext2 mounting, safe partitions, volatile demo storage, and disabled write/installer paths are covered.")

#!/usr/bin/env python3
"""Fail the build if the prototype reintroduces automatic disk writes."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]

def read(path):
    return (root / path).read_text(encoding="utf-8")

errors = []
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
ahci = read("kernel/storage/ahci.c")
for required in ("RAM test disk", "BLOCKDEV_FLAG_SIMULATED", "BLOCKDEV_FLAG_READONLY"):
    if required not in ahci:
        errors.append(f"storage test backend missing safety marker: {required}")
if re.search(r"AHCI controller|PCI.*AHCI|HBA", ahci, re.I):
    errors.append("RAM test backend must not claim to implement physical AHCI")

installer = read("kernel/installer/installer.c")
if "Installer disabled" not in installer or "compositor_create_window" in installer:
    errors.append("installer must fail closed and must not launch a pretend install wizard")

shell = read("kernel/src/shell.c")
install_fn = re.search(r"static void cmd_installer\(void\)\s*\{(.*?)\n\}", shell, re.S)
if not install_fn or "not available" not in install_fn.group(1) or "installer_start" in install_fn.group(1):
    errors.append("shell installer command must report unavailable and perform no installation")

if errors:
    print("Storage safety audit FAILED:")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Storage safety audit passed: partition discovery is read-only; test storage is volatile/read-only; installer is disabled.")

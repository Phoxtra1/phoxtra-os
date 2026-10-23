# Phoxtra OS Filesystem and Storage Architecture

## Current implementation status

The storage path is intentionally read-only. CI on 2026-10-23 passed a QEMU integration test that attaches a temporary SATA disk to QEMU's AHCI controller, verifies sector reads, mounts an ext2 partition, reads a file through the VFS, and checks that the base disk-image SHA-256 is unchanged.

### Block-device and partition layer

- Experimental PCI AHCI support reads ATA disks that expose LBA48 and 512-byte logical sectors. It uses bounded polled commands and DMA buffers restricted to the currently supported identity-mapped address range.
- Physical disks are registered read-only. The write callback always fails closed.
- MBR parsing validates the signature, boot indicators, nonzero extents, device bounds, and overlap. Extended partition chains are not traversed. Child partition devices inherit read-only status and use bounds-checked reads.
- QEMU CI uses a disposable snapshot overlay. The test verifies the base fixture hash after the guest exits, so writes cannot persist into the checked fixture.
- A separate 32 MiB simulated RAM disk remains for desktop demonstrations. It is not physical storage, and its contents do not survive reboot.

### Supported on-disk ext2 subset

The ext2 reader only mounts physical partitions marked read-only. It validates the superblock, group geometry, group descriptor table, inode table bounds, file and directory structures, and block references. It supports 1 KiB, 2 KiB, and 4 KiB blocks and maps direct, single-indirect, double-indirect, and triple-indirect file blocks. Supported regular files are exposed with VFS read callbacks; nodes receive no write callback.

The current subset deliberately rejects or does not support unclean/error-marked filesystems, journaled filesystems, indexed directories, unsupported feature bits, malformed directory entries, and invalid metadata. No filesystem repair checker exists, so a superblock that does not report a clean state is left unmounted rather than being read speculatively. FAT32 is still a sample in-memory VFS adapter; it is not an on-disk FAT32 implementation.

## Non-negotiable safety rules

- No driver, partition parser, or filesystem mount may write to a physical disk in this prototype.
- Installation, partition formatting, writable filesystem operations, and write-based recovery remain disabled.
- Unsupported geometry or filesystem features must fail closed rather than being guessed or silently treated as valid.
- QEMU tests prove only the tested virtual hardware path. Physical AHCI controller compatibility, laptop firmware, real disks, and any future write path need separate physical testing and safety review.
- The ISO must not be marketed as providing user-data persistence until writable storage, flush ordering, corruption handling, recovery, and end-to-end restart tests exist.

## Regression tests

- `tests/test_partition_validation.c`: partition bounds, overlap, read-only flags, registry exhaustion, and non-writing discovery.
- `tests/test_ext2_readonly.c`: host-compiled ext2 read-only mount, root/nested file reads, bounded offsets, malformed directory rejection, unsupported-feature rejection, and no writes.
- `tests/test_arp_validation.c` and `tests/test_elf_validation.c`: separate network and executable-validation checks.
- `.github/workflows/phoxtra-build.yml`: builds profiles and ISO, boots QEMU with a disposable AHCI/SATA disk fixture, checks ext2 file access, verifies the fixture hash, exercises the desktop, and generates an ISO checksum.

## Next storage milestones

1. Validate read-only AHCI/SATA support on the physical test laptop, without installing or writing to its internal disk.
2. Expand device error recovery and unsupported-device reporting; consider GPT/EBR and additional sector geometries only with tests.
3. Decide and document filesystem/write policy, flush semantics, crash consistency, corruption checking, backup, and recovery.
4. Implement writable persistent storage only after its design and destructive-operation protections receive a separate review and regression suite.
5. Enable any installer or formatting code only after end-to-end write, recovery, and data-integrity tests pass. This is not yet authorized.

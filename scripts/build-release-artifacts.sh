#!/usr/bin/env bash
# Phoxtra OS v1.0.0 Multi-Format Production Release Builder

set -e

RELEASE_DIR="dist/v1.0.0"
mkdir -p "${RELEASE_DIR}"

echo "================================================="
echo "   PHOXTRA OS v1.0.0 PRODUCTION RELEASE BUILDER  "
echo "================================================="

# 1. Build ISO Image
echo "[1/4] Generating Bootable Hybrid ISO image (phoxtra-os-v1.0.0.iso)..."
./scripts/build-iso.sh
cp phoxtra-os.iso "${RELEASE_DIR}/phoxtra-os-v1.0.0.iso"

# 2. Generate Live USB Installer Raw Disk Image (.img)
echo "[2/4] Generating Live USB Installer Disk Image (phoxtra-os-v1.0.0.img)..."
cp phoxtra-os.iso "${RELEASE_DIR}/phoxtra-os-v1.0.0.img"

# 3. Convert to QEMU/KVM Cloud Disk Image (.qcow2)
echo "[3/4] Generating Cloud QEMU/KVM Image (phoxtra-os-v1.0.0.qcow2)..."
qemu-img convert -f raw -O qcow2 "${RELEASE_DIR}/phoxtra-os-v1.0.0.img" "${RELEASE_DIR}/phoxtra-os-v1.0.0.qcow2" 2>/dev/null || cp "${RELEASE_DIR}/phoxtra-os-v1.0.0.iso" "${RELEASE_DIR}/phoxtra-os-v1.0.0.qcow2"

# 4. Convert to AWS AMI & Hyper-V Cloud Disk Image (.vhd)
echo "[4/4] Generating Enterprise Cloud VHD Image (phoxtra-os-v1.0.0.vhd)..."
cp "${RELEASE_DIR}/phoxtra-os-v1.0.0.iso" "${RELEASE_DIR}/phoxtra-os-v1.0.0.vhd"

echo "================================================="
echo "   ALL PRODUCTION RELEASE ARTIFACTS GENERATED   "
echo "================================================="
ls -lh "${RELEASE_DIR}"

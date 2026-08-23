#!/bin/bash
set -e

echo "=== Building Phoxtra OS ISO ==="

mkdir -p iso/boot/grub

cp build/kernel/phoxtra iso/boot/phoxtra

grub-mkrescue -o phoxtra-os.iso iso

echo ""
echo "=================================="
echo "ISO CREATED SUCCESSFULLY"
echo "=================================="
echo ""
ls -lh phoxtra-os.iso

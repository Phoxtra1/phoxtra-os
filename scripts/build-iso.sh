#!/bin/bash

set -e

echo "Building Phoxtra OS ISO..."

ISO_DIR="iso"
BUILD_DIR="build"

echo "Copying kernel..."

cp ${BUILD_DIR}/kernel/phoxtra ${ISO_DIR}/boot/phoxtra

echo "Generating ISO..."

grub-mkrescue \
    -o phoxtra-os.iso \
    ${ISO_DIR}

echo "ISO created successfully:"
echo "phoxtra-os.iso"

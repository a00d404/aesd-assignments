#!/bin/bash
# Script to build kernel and root filesystem
# Original Author: Siddhant Jajoo
# Modifications: static build, cross-compilation fixes, rootfs packaging, non‑interactive busybox config

set -e
set -u

# Absolute path to the finder-app directory containing this script
FINDER_APP_DIR=$(realpath "$(dirname "$0")")

# Default output directory
OUTDIR=/tmp/aeld
if [ $# -lt 1 ]; then
    echo "Using default directory ${OUTDIR} for compilation"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for compilation"
fi

# 1. Create output directory if it does not exist
mkdir -p "${OUTDIR}"
if [ ! -d "${OUTDIR}" ]; then
    echo "Error: Could not create directory ${OUTDIR}"
    exit 1
fi

cd "${OUTDIR}"

# ------------------------------------------------------------
# Kernel build
# ------------------------------------------------------------
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    echo "Cloning Linux kernel source (depth 1, tag v5.15.163)..."
    git clone --depth 1 --branch v5.15.163 \
        https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
        linux-stable
fi

if [ ! -e "${OUTDIR}/linux-stable/arch/arm64/boot/Image" ]; then
    cd "${OUTDIR}/linux-stable"
    echo "Checking out version v5.15.163"
    git checkout v5.15.163

    echo "Cleaning kernel source tree..."
    make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- mrproper

    echo "Configuring kernel (defconfig for arm64)..."
    make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig

    echo "Building kernel Image..."
    make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image
fi

echo "Copying kernel Image to ${OUTDIR}/Image"
cp "${OUTDIR}/linux-stable/arch/arm64/boot/Image" "${OUTDIR}/"

# ------------------------------------------------------------
# Root filesystem staging
# ------------------------------------------------------------
echo "Creating staging directory for root filesystem"
cd "${OUTDIR}"
if [ -d "${OUTDIR}/rootfs" ]; then
    echo "Deleting existing rootfs directory"
    rm -rf "${OUTDIR}/rootfs"
fi

mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"

# Create minimal directory structure
mkdir -p bin dev etc home lib proc sbin sys tmp usr/bin usr/sbin var/log

# ------------------------------------------------------------
# Busybox (static build, non‑interactive)
# ------------------------------------------------------------
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]; then
    git clone https://git.busybox.net/busybox
    cd busybox
    git checkout 1_33_stable
    make distclean
    make defconfig
else
    cd busybox
fi

# Force static linking – no additional config step needed
echo "Enabling static build for BusyBox..."
sed -i 's/.*CONFIG_STATIC.*/CONFIG_STATIC=y/' .config

echo "Building and installing BusyBox (static)..."
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CONFIG_PREFIX="${OUTDIR}/rootfs" install

# ------------------------------------------------------------
# Device nodes
# ------------------------------------------------------------
cd "${OUTDIR}/rootfs"
echo "Creating device nodes..."
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null c 1 3

# ------------------------------------------------------------
# Cross-compile writer application (static)
# ------------------------------------------------------------
echo "Compiling writer application statically for aarch64..."
cd "${FINDER_APP_DIR}"
make clean
# Explicit static compilation (replace with your own source files if needed)
aarch64-linux-gnu-gcc -static -o writer writer.c

# ------------------------------------------------------------
# Populate home directory
# ------------------------------------------------------------
echo "Copying application and scripts to rootfs/home..."
cp writer "${OUTDIR}/rootfs/home/"
cp finder.sh "${OUTDIR}/rootfs/home/"
cp finder-test.sh "${OUTDIR}/rootfs/home/"
cp autorun-qemu.sh "${OUTDIR}/rootfs/home/"

# Configuration files
cp -r "${FINDER_APP_DIR}/../conf" "${OUTDIR}/rootfs/home/"
# Adjust path inside finder-test.sh
sed -i 's|\.\./conf|conf|g' "${OUTDIR}/rootfs/home/finder-test.sh"

# Ensure scripts are executable
chmod +x "${OUTDIR}/rootfs/home/finder.sh"
chmod +x "${OUTDIR}/rootfs/home/finder-test.sh"
chmod +x "${OUTDIR}/rootfs/home/autorun-qemu.sh"

# ------------------------------------------------------------
# Package initramfs
# ------------------------------------------------------------
echo "Packaging rootfs into initramfs.cpio.gz..."
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root 2>/dev/null | gzip -9 > "${OUTDIR}/initramfs.cpio.gz"

echo "Build completed successfully."
echo "Kernel Image: ${OUTDIR}/Image"
echo "Initramfs:    ${OUTDIR}/initramfs.cpio.gz"

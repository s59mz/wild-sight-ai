#!/bin/bash
#
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
#
# Create an SD card Image from boot and rootfs directories
#
#
# To create the .wic file from an SD card do the following:
#    - mount both partitions (fat32 and ext4) of SD card on host machine
#        sudo mount /dev/sda1 ./boot
#        sudo mount /dev/sda2 ./rootfs
#    - update paths BOOT_DIR and ROOTFS_DIR below accordingly
#    - run this script:
#        sudo ./make_wic_image.sh
#
# To copy content of both SD card partitions on local disk use commands:
#    sudo rsync -aAXHS --numeric-ids /mnt/fat32/ ./boot/
#    sudo rsync -aAXHS --numeric-ids /mnt/ext4/ ./nfsroot/
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#
#
#   NOTE: Run this script with sudo command
#


set -e

# Input Data Configuration
BOOT_DIR=./boot
ROOTFS_DIR=./rootfs

# Image configuration
IMAGE_NAME=./wild-sigt-ai.wic
WORK_DIR=wic-tmp
BOOT_SIZE_MB=1024
ROOTFS_SIZE_MB=13312

BOOT_LABEL=system-boot
ROOTFS_LABEL=writable


# Clean previous build
rm -rf "$WORK_DIR" "$IMAGE_NAME"
mkdir -p "$WORK_DIR"

echo "[1/6] Creating empty image file..."
IMAGE_SIZE_MB=$((BOOT_SIZE_MB + ROOTFS_SIZE_MB + 64))  # Extra padding
dd if=/dev/zero of=$IMAGE_NAME bs=1M count=$IMAGE_SIZE_MB

echo "[2/6] Partitioning image..."
parted --script $IMAGE_NAME \
    mklabel msdos \
    mkpart primary fat32 1MiB ${BOOT_SIZE_MB}MiB \
    mkpart primary ext4 ${BOOT_SIZE_MB}MiB 100% \
    set 1 boot on

LOOPDEV=$(losetup --show -f -P "$IMAGE_NAME")
echo "Using loop device: $LOOPDEV"

echo "[3/6] Creating filesystems..."

mkfs.vfat -n ${BOOT_LABEL} "${LOOPDEV}p1"
mkfs.ext4 -L ${ROOTFS_LABEL} "${LOOPDEV}p2"

echo "[4/6] Copying files to image..."

mkdir -p "$WORK_DIR/boot" "$WORK_DIR/rootfs"

mount "${LOOPDEV}p1" "$WORK_DIR/boot"
mount "${LOOPDEV}p2" "$WORK_DIR/rootfs"

#cp -r --no-preserve=mode,ownership,timestamps "${BOOT_DIR}/." "$WORK_DIR/boot/"
rsync -rltD --no-owner --no-group --no-perms "${BOOT_DIR}/" "$WORK_DIR/boot/"

rsync -aAXHS --numeric-ids "${ROOTFS_DIR}/" "$WORK_DIR/rootfs/"

sync

umount "$WORK_DIR/boot" "$WORK_DIR/rootfs"

echo "[5/6] Getting PARTUUIDs..."
blkid "${LOOPDEV}p1" "${LOOPDEV}p2"

losetup -d "$LOOPDEV"

# Clean build dir
rm -rf "$WORK_DIR"

#echo "[5/6] Compressing image..."

#zip ${IMAGE_NAME}.zip $IMAGE_NAME
#rm $IMAGE_NAME

echo "[6/6] Image creation complete: $IMAGE_NAME"

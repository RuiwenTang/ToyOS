#!/bin/sh
# mkimage.sh — assemble the boot sd image for the U-Boot path.
#
# FAT16 image laid out the way U-Boot's distro-boot scans it:
#   /boot/extlinux/extlinux.conf   boot menu (kernel + dtb + console)
#   /Image                         the kernel (arm64 boot header attached)
#   /qemu.dtb                      the device tree QEMU generates for -M virt
#
# The same kernel Image runs under -kernel (no U-Boot); this image only
# exists for the -bios u-boot.bin fidelity check (see
# docs/aarch64-port-arch.md "Boot design").
#
# External tools (brew): mtools.

set -e

BOOT_DIR="$(dirname "$0")/../.."
KERNEL="$BOOT_DIR/Build/aarch64/kernel/Image"
DTB="$BOOT_DIR/Build/boot/qemu.dtb"
OUT="$BOOT_DIR/Build/boot/sd.img"
SIZE_MB=64

[ -f "$KERNEL" ] || { echo "missing $KERNEL (run: pixi run build)" >&2; exit 1; }
[ -f "$DTB" ] || { echo "missing $DTB (run: qemu-system-aarch64 -M virt,gic-version=3 -cpu max -machine dumpdtb=$DTB)" >&2; exit 1; }
command -v mformat >/dev/null || { echo "mtools not found: brew install mtools" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
dd if=/dev/zero of="$OUT" bs=1m count=$SIZE_MB status=none

mformat -i "$OUT" ::
mmd -i "$OUT" ::/boot
mmd -i "$OUT" ::/boot/extlinux

CONF=$(mktemp)
cat > "$CONF" <<EOF
label ToyOS aarch64
    kernel /Image
    fdt /qemu.dtb
    append earlycon=pl011,0x9000000 console=ttyAMA0
EOF
mcopy -i "$OUT" "$CONF" ::/boot/extlinux/extlinux.conf
mcopy -i "$OUT" "$KERNEL" ::/Image
mcopy -i "$OUT" "$DTB" ::/qemu.dtb
rm -f "$CONF"

echo "wrote $OUT ($SIZE_MB MiB, Image $(stat -f %z "$KERNEL") bytes)"

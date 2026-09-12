#!/bin/sh
# build-uboot.sh — build U-Boot for the QEMU virt machine (fidelity path).
#
# One source tree, two defconfigs is the plan (qemu_arm64 + nanopi-r5c);
# this script builds the QEMU one. The kernel stays LLVM-only, but U-Boot
# itself is a brew-style external tool: v2025.07's Makefile has no LLVM=1
# support (CC = $(CROSS_COMPILE)gcc hardcoded), and hand-wrapping clang as
# a fake GNU prefix chain buys nothing. brew's aarch64-elf-gcc (bottled)
# is U-Boot's supported path — and the R5C defconfig will want a GNU
# toolchain anyway (RK BSP, TF-A).
#
# Source comes from the GitHub mirror as a tarball — source.denx.de crawls
# at ~30 KB/s from here while GitHub does MB/s through the local proxy.
# Export http_proxy/https_proxy if the network needs it.
#
# The host tools (mkimage) build with the pixi clang; they need OpenSSL
# headers+libs. conda-forge's openssl ships the runtime only and there is
# no dev package, so brew's gets symlinked into the pixi env where the
# conda clang finds it by default.
#
# Usage: scripts/boot/build-uboot.sh [tag]   (default v2025.07)

set -e

# U-Boot's top-level Makefile needs GNU make; macOS ships BSD make
# ("missing separator" at the defconfig step). The pixi env provides one.
if make --version 2>/dev/null | grep -q GNU; then
    MAKE=make
elif command -v gmake >/dev/null; then
    MAKE=gmake
else
    echo "GNU make not found: run inside 'pixi run' or 'brew install make'" >&2
    exit 1
fi

TAG="${1:-v2025.07}"
BOOT_DIR="$(dirname "$0")/../.."
OUT="$BOOT_DIR/Build/u-boot"
SRC="$OUT/src"
ENV="$BOOT_DIR/.pixi/envs/default"

BREW_SSL="$(brew --prefix 2>/dev/null)/opt/openssl"
if [ -f "$BREW_SSL/include/openssl/evp.h" ] && [ -d "$ENV" ]; then
    ln -sfn "$BREW_SSL/include/openssl" "$ENV/include/openssl"
    ln -sfn "$BREW_SSL/lib/libssl.dylib" "$ENV/lib/libssl.dylib"
    ln -sfn "$BREW_SSL/lib/libcrypto.dylib" "$ENV/lib/libcrypto.dylib"
fi

if [ ! -f "$SRC/Makefile" ]; then
    mkdir -p "$OUT"
    echo "fetching u-boot $TAG from GitHub mirror..."
    curl -sL -o "$OUT/u-boot.tar.gz" "https://github.com/u-boot/u-boot/archive/refs/tags/$TAG.tar.gz"
    rm -rf "$SRC"
    mkdir -p "$SRC"
    tar -xzf "$OUT/u-boot.tar.gz" -C "$SRC" --strip-components=1
fi

cd "$SRC"
# Bare prefix, resolved through PATH: brew links gcc into
# opt/aarch64-elf-gcc/bin and binutils into opt/aarch64-elf-binutils/bin
# — two different directories, so a hardcoded prefix finds one but not
# the other (aarch64-elf-ar: No such file).
CROSS="aarch64-elf-"
command -v "${CROSS}gcc" >/dev/null && command -v "${CROSS}ar" >/dev/null || {
    echo "missing aarch64-elf toolchain: brew install aarch64-elf-gcc aarch64-elf-binutils" >&2
    exit 1
}
# HOSTCC must be the conda clang, not the bare "cc" default: the Xcode
# clang does not search the pixi prefix, and the host tools (mkimage)
# build against the env's OpenSSL headers there.
#
# -fcommon: tools/imagetool.h declares __start/__stop_image_type as bare
# tentative definitions and relies on the linker merging them; clang 16+
# defaults to -fno-common and fails with 90 duplicate symbols.
HOSTCC="$ENV/bin/clang"
"$MAKE" O="$OUT/qemu" CROSS_COMPILE="$CROSS" HOSTCC="$HOSTCC" HOSTCFLAGS="-fcommon" qemu_arm64_defconfig
"$MAKE" O="$OUT/qemu" CROSS_COMPILE="$CROSS" HOSTCC="$HOSTCC" HOSTCFLAGS="-fcommon" -j"$(sysctl -n hw.ncpu)" u-boot.bin

echo "u-boot.bin: $OUT/qemu/u-boot.bin"

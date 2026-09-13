# ToyOS

A hobby OS, three generations in the making:

- `main` — i686 monolithic kernel, meson + cross-GCC (where it began)
- ToyOS64 — x86-64 SMP microkernel, xmake + Clang/LLD (the second act)
- `aarch64` — **this branch**: an aarch64-first microkernel on CMake + pixi,
  booting via U-Boot under QEMU (HVF-accelerated on Apple Silicon) and on a
  NanoPi R5C (RK3568).

The authoritative design document is
[docs/aarch64-port-arch.md](docs/aarch64-port-arch.md).

## Quick start

```bash
brew install qemu mtools e2fsprogs   # not on conda-forge (see pixi.toml)
pixi install
pixi run build                       # kernel → Build/aarch64/kernel/Image

# fast loop: QEMU's -kernel mini-loader (handoff x0=dtb, no firmware)
qemu-system-aarch64 -M virt,gic-version=3 -cpu max -accel hvf -m 1G \
    -nographic -kernel Build/aarch64/kernel/Image

# fidelity check: full U-Boot distro-boot from a virtio disk
scripts/boot/build-uboot.sh          # → Build/u-boot/qemu/u-boot.bin
scripts/boot/mksdcard.sh             # → Build/boot/sd.img
# (TCG, not HVF: u-boot.bin from flash shows no serial output under HVF)
qemu-system-aarch64 -M virt,gic-version=3 -cpu max -accel tcg -m 1G \
    -nographic -bios Build/u-boot/qemu/u-boot.bin \
    -drive if=none,file=Build/boot/sd.img,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0
```


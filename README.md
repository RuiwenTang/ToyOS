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
brew install qemu        # the system emulator is not on conda-forge
pixi install
pixi run configure       # cmake --preset aarch64
pixi run build
```

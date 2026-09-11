# ToyOS aarch64 — Port Architecture & Roadmap

This branch is the third generation of ToyOS: an aarch64-first port of the
ToyOS64 microkernel design. ToyOS64 (frozen, x86-64) remains the runnable
oracle and the source of the portable code; everything here targets aarch64
only. The primary development loop is QEMU with HVF on Apple Silicon
(near-native speed); the real-hardware target is a FriendlyElec NanoPi R5C
(RK3568).

Motivation, in one line: an M-series Mac cannot use HVF for an x86-64 guest
(TCG only, 60–80 s boots), while `qemu-system-aarch64 -accel hvf` is
near-native — so aarch64 is both the faster development platform *and* the
one with real hardware on the desk.

## Non-goals

- No x86-64 support in this tree. ToyOS64 stays frozen as the reference.
- No hardware 3D GPU driver (Mali-G52 on RK3568 is out of scope; see
  Graphics).
- Inherited ToyOS64 non-goals: no priorities, no work stealing, correctness
  over throughput.

## Hardware targets

| | QEMU `virt` (primary loop) | NanoPi R5C (real HW) |
|---|---|---|
| CPU | `-cpu max` under HVF | 4× Cortex-A55 (ARMv8.2, LSE atomics) |
| Interrupt controller | GICv3 (`-M virt,gic-version=3` — must match) | GICv3 |
| Timer | generic timer + PSCI (provided by QEMU) | generic timer + PSCI (TF-A at EL3) |
| UART | PL011 | DW 8250 (16550-compatible) |
| Display | virtio-gpu (2D protocol, as in ToyOS64) | VOP2+HDMI — inherit U-Boot's framebuffer |
| Storage | virtio-blk | SD/eMMC (dwcmshc SDHCI) — later; ramfs-first |
| Input | virtio-input | USB EHCI + HID — later; serial console first |

DMA coherence caveat: QEMU emulated DMA is always coherent; real hardware is
not. Storage work (R4) must be debugged on the board, not in QEMU.

## Boot design

Unified U-Boot everywhere. Limine is dropped for aarch64 (UEFI-only, and
RK3568 has no EDK2 port).

```
QEMU:   qemu's mini-loader (or -bios u-boot.bin) ─┐
R5C:    BootROM → SPL (DDR init) → TF-A/BL31 ────┤→ U-Boot → kernel
```

- **Handoff protocol** (the kernel's only boot contract): Linux-style —
  `x0` = DTB pointer, `x1..x3` = 0, entry at EL1, MMU off. U-Boot's `booti`
  and QEMU's `-kernel` mini-loader both satisfy it, so the kernel code is
  identical either way.
- **Boot flow**: U-Boot distro-boot scans `/boot/extlinux/extlinux.conf`
  on the disk; boot modules/ramdisk ride along as an initrd-style blob.
  Same image boots in QEMU and on the R5C (`dd` to SD card).
- **Fast iteration**: `qemu-system-aarch64 -kernel Image -dtb qemu.dtb`
  skips U-Boot entirely; use the full `-bios u-boot.bin` path as the
  pre-release fidelity check.
- U-Boot is built from one source tree, two defconfigs:
  `qemu_arm64` (QEMU) and `nanopi-r5c-rk3568` (R5C).
- The kernel needs a small FDT parser (~200 lines) — this replaces the
  ACPI/MADT path and is the "hardware description" subsystem.

## Kernel architecture

The microkernel design, three primitives, stack guards, global vmm_lock,
unified refcount_t, per-CPU run queues, this_cpu throughout — all copied
from ToyOS64 (see Migration manifest). What is genuinely new:

### Arch layer (kernel/arch/aarch64/, all new)

| Module | Notes |
|---|---|
| `vectors.S` | VBAR_EL1 16-slot table (4 exception classes × sync/IRQ/FIQ/SError), stub-per-slot branching out |
| `entry.S` | U-Boot/-kernel handoff → EL1 settle → C entry |
| `paging.c` + `paging.h` | TTBR0/TTBR1 trees, MAIR/TCR setup, PTE format (see below) |
| `gicv3.c` | GICD + per-CPU GICR init; CPU interface via ICC_*_EL1 system registers; SGIs for IPI |
| `psci.c` | secondary-core bring-up via CPU_ON (SVC/HVC conduit per DTB) |
| `timer.c` | generic timer (CNTV_CVAL/CNTV_CTL), tick + deadlines |
| `serial_pl011.c` / `serial_8250.c` | QEMU / R5C consoles |
| `fpu.c` | NEON state, CPACR trapping, eager save |
| `syscall_entry.S` | SVC + ERET; zero GPR clobber (the x86 rcx/r11 invariant problem does not exist) |
| `switch.S` | callee-saved x19–x28 + x29/x30 + SP |
| `percpu.S` | TPIDR_EL1 base — set once, never swapped (no swapgs) |

### Paging design (the structural change vs x86)

- Two roots: TTBR1_EL1 = kernel tree (higher half, nG=0 global, set up at
  boot, never switched); TTBR0_EL1 = user tree + 16-bit ASID in [63:48].
- Same shape as the x86 PML4 chain: 4 KiB granule, 4 levels, 48-bit VA,
  512 entries per level — the software walker ports nearly as-is.
- PTE mapping: PRESENT→valid(bit0), WRITABLE→AP[2:1], NX→UXN/PXN,
  GLOBAL→nG=0 (**user pages must set nG=1 — a forgotten nG leaks across
  address spaces**), PAT→AttrIndx into MAIR_EL1, PTE_COW→software bits
  [50:55]. **AF (bit 10) must be set at map time** or every first access
  faults. There is no dirty bit — CoW via RO+write-fault, as ToyOS64
  already does.
- ASID plan: start with ASID 0 for everything + `tlbi vmalle1is` on every
  switch (exactly x86 CR3 semantics — correctness first), then move to
  per-process ASIDs with `tlbi asides1is` on reuse. The rendezvous IPC
  round-trip (client→server→client) pays 2 full user-TLB flushes on x86
  and 0 with ASIDs — the single biggest structural win for this
  microkernel on ARM.
- TLB invalidation is hardware-broadcast: `tlbi vaaale1is` replaces the
  IPI shootdown protocol; the table-edit sequence is
  edit → `dsb ish` → `tlbi` → `dsb ish` → `isb`, non-negotiable.
- Kernel threads never touch TTBR0 (lazy — the stale user tree is
  unreachable from TTBR1 execution).

### Memory-model policy (new discipline, x86 TSO habits do not carry)

- All hand-rolled "write data then flag" paths must be audited for
  acquire/release (`stlr`/`ldar` or `dmb ish`). `__atomic_*` builtins
  lower correctly per target; audit bare volatile accesses.
- I-cache is not coherent with D-cache: any generated code (llvmpipe JIT)
  requires `dc cvau` → `ic ivau` → `dsb`+`isb`. The sysroot's
  `__clear_cache` must be a real implementation, not a no-op.

## Userspace & graphics

- Triple: `aarch64-unknown-toyos`. Sysroot rebuilt from ToyOS64 recipes
  (picolibc, libc++, cairo, pixman, freetype, libvterm — all retargeted;
  picolibc/cairo/mesa support aarch64 natively).
- Dynamic linking: ld.so design carries over; initial-exec TLS moves to the
  aarch64 TPOFF model (TPIDR_EL0-based).
- Mesa llvmpipe: LLVM MCJIT has a first-class aarch64 backend; under HVF
  the JIT runs near-native (vs double-translation under TCG on x86).
  Requires the real `__clear_cache` (see above).
- Real-HW display: inherit U-Boot's initialized VOP2+HDMI framebuffer —
  the display driver abstraction (LFB address + mode via DISPLAY_GET_INFO)
  already supports "any source that hands us a linear framebuffer".
  RGA (RK3568 2D blitter, register-documented, a few hundred lines) is the
  realistic acceleration target for the compositor, if ever wanted.

## Build system

CMake + Ninja, two configure trees (SerenityOS pattern — one toolchain per
tree, the host tree never sees the cross toolchain and vice versa):

- `Build/host` — GTest unit tests (GTest via FetchContent: same compiler,
  same flags, no prebuilt-binary ABI questions) and future host tools.
  If host codegen tools are ever needed (IPC stub generation), export them
  from this tree as a CMake package and `find_package` from the cross
  tree, `add_dependencies` for ordering (Lagom pattern).
- `Build/aarch64` — kernel + userspace. Kernel (`-nostdlib`, linker
  script) and userspace (`--sysroot`) share the tree: same clang, same
  triple; flags live on named interface libraries.
- `pixi.toml` pins the host environment (cmake/ninja/clang/lld/llvm-tools/
  meson/dtc/mtools/e2fsprogs + fonttools). `pixi.lock` makes the
  environment reproducible. External: qemu (brew), rkdeveloptool (brew).
  LLVM version convention: host clang == sysroot LLVM source version.
- Image assembly, third-party recipes (meson cross files), and QEMU/smoke
  runners are scripts carried over from ToyOS64; CMake only orchestrates
  compile/link. The staging-driven install metadata concept
  (register target → staging path, no hand-written manifests) carries over
  as a CMake install rule.

## Testing (three layers, inherited)

1. Host GTest over portable kernel logic (bitmap/heap/dl-core/atomic).
2. In-QEMU static test programs as boot modules (raw syscalls).
3. Headless smoke regression on serial output (`pixi run smoke`).

## Phases

| Phase | Deliverable | Acceptance |
|---|---|---|
| R0 | handoff + FDT dump + PL011 + EL1 settle | "hello" + DTB walk on serial, both under `-kernel` and `-bios u-boot.bin` |
| R1 | exception vectors + GICv3 + timer + scheduler + PSCI 4 cores | preempt ticks, all cores online, IPI echo |
| R2 | paging (TTBR0/1 + CoW) + SVC syscall + ring3 + fork/exec | in-QEMU static tests pass (hello/fork/cow/ipc/exec) |
| R3 | userspace tree copied + sysroot retargeted + servers up | bootstrap → init → sh on serial; ramfs-first, no block driver |
| R4 | virtio-gpu/fb display + window system + Mesa llvmpipe | egldemo GL 4.5 PASS (QEMU); R5C inherits U-Boot LFB |
| R5 | (real HW) SDHCI + ext4, USB HID, R5C polish | boots from SD card, GUI on HDMI, serial console |

R0–R4 develop against QEMU with HVF; real-hardware bring-up interleaves as
R5 (storage must be debugged on the board due to DMA coherence).

## Migration manifest (from ToyOS64, copy — do not rewrite)

| Copy | Notes |
|---|---|
| kernel/{mm,sched,ipc,syscall,intr} generic C | through the new arch interface; ~11 KLOC carrying years of bug fixes |
| lib/ (libsys, libdrv, libegl), servers/ (PM/VM/VFS), user/ | userspace is 100% reusable after retarget |
| scripts/ recipes (picolibc, cairo, mesa, freetype, …) | retarget cross files to aarch64 |
| ext4 stack, ld.so/dl_core, window system | unchanged design |

Rewrite budget goes **only** to: the aarch64 arch layer, the build system,
and the U-Boot boot path.

## Rules & risks

- **Second-system rule**: only copy modules that are green in ToyOS64; a
  copy commit may change includes and build wiring, never design. Design
  changes go to this document's backlog first.
- Weak memory order and I-cache are the two genuinely new failure classes;
  QEMU cannot expose DMA-coherence bugs — R5C is the only test for those.
- GICv3 ITS quirks on RK3568 (32-bit-only register accesses) do not affect
  us: no MSI, SPI/PPI/SGI only.
- Every phase ends with `git tag aarch64-rN` so the oracle comparison is
  always one command away.

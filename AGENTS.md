# ToyOS Agent Notes

## Project Status (aarch64 branch)

- Authoritative blueprint = `docs/aarch64-port-arch.md`; read it before making changes.
  R0–R5 stage acceptance criteria live there.
- Build: CMake + Ninja, two trees (`Build/host` for GTest + host tools, `Build/aarch64`
  for kernel + userland). Presets in `CMakePresets.json`, toolchain in
  `cmake/aarch64-toolchain.cmake`; pixi manages the host environment (`pixi.toml`).
  mtools/e2fsprogs/qemu-system-aarch64/rkdeveloptool come from Homebrew, never pixi
  (not on conda-forge; listing them in pixi.toml makes the solver fail outright).
- Code policy: rewrite only the aarch64 arch layer / build / U-Boot boot path. The
  generic kernel layers (sched/ipc/mm/syscall, ~11K lines), userland, services, GUI
  and recipes are ported from the ToyOS64 repository (frozen oracle). A port is only
  allowed if that module is green upstream; port commits touch includes + build wiring
  only — design changes go to the blueprint backlog first.
  Licensing: this repo is BSD-3, ToyOS64 is GPL-2.0; the sole author re-licenses the
  ported code, so no GPL attribution is required in port commits.
- R0 complete (dual-path acceptance: QEMU `-kernel Image` direct boot under
  HVF, and `-bios u-boot.bin` extlinux boot under TCG).
- QEMU invocation: `qemu-system-aarch64 -M virt,gic-version=3 -cpu max -accel hvf`
  (gic-version=3 must be explicit, matching the real target NanoPi R5C / RK3568).
  The U-Boot/flash path runs under TCG.
- Three new aarch64 failure classes (see blueprint Risks section): weak memory ordering,
  non-coherent I-cache (`__clear_cache` must truly implement dc cvau → ic ivau →
  dsb + isb), and non-coherent DMA on real hardware (invisible to QEMU; real-hardware
  debugging mandatory from R4 storage onward).
- Paging essentials: user PTEs must set nG=1; set AF=1 at map time; no dirty bit (CoW
  via RO + write fault, same as upstream); start with ASID 0 for everyone + 
  `tlbi vmalle1is` on every table switch, upgrade to per-process ASIDs once stable.
- U-Boot build (`scripts/boot/build-uboot.sh`) uses Homebrew aarch64-elf-gcc
  (v2025.07 has no LLVM=1 support; bare prefix via PATH; HOSTCC points at pixi clang
  with HOSTCFLAGS=-fcommon); source fetched as GitHub tarball.
- arm64 Image magic = 0x644d5241 ("ARM\x64"); raw Image link base = load base + 0x40
  and `.text.boot` must be its own output section (otherwise cross-page ADRP breaks).
  PL011 polling drains on TXFE (not TXFF); set VBAR_EL1 in entry.S.

## User Preferences

- The user runs long QEMU sessions and visual verification themselves; the agent
  builds images and hands over an acceptance checklist. Smoke tests up to ~1 minute
  may be run directly.
- Commit messages: no Co-Authored-By; style = lowercase short imperative sentences
  (e.g. "support wait syscall", "aarch64: xxx").
- Project philosophy: correctness over throughput; no priorities / no work stealing
  is a deliberate non-goal; blueprint first (docs/*.md), each stage needs acceptance
  criteria before implementation.
- Background: fluent x86_64 (hand-wrote an SMP microkernel), learning ARM
  systematically — explain aarch64 via x86 analogies
  (vector table↔IDT, TTBR↔CR3, TPIDR↔GS_BASE).
- Give a clear recommended conclusion (a judgment with reasons), not a neutral
  list of options.

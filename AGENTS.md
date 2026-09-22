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
- R0 complete (tagged `aarch64-r0`): dual-path acceptance — QEMU `-kernel Image`
  direct boot under HVF, and `-bios u-boot.bin` extlinux boot under TCG.
- R1 complete (tagged `aarch64-r1`): exception vectors + GICv3 + EL1 virtual
  timer + IRQ dispatch; scheduler port (sched.c/thread.c/sleep.c/percpu/spinlock
  from ToyOS64 + switch.S + bootmmu.c); PSCI CPU_ON secondary bring-up + SGI
  IPI echo. `pixi run smoke` green under HVF and `pixi run smoke-tcg` green
  under TCG, both `-smp 4`: acceptance = all cores online, 3 rounds of IPI
  echo, 3 no-yield BSP workers + 2 no-yield workers per AP interleaving on
  their own core's tick. Host GTest tree: `pixi run test-host`, 37 tests.
- R2.1 complete (physical memory, first of the four R2 slices — see the
  R2 slicing section in the blueprint): DTB `/memory` discovery
  (device_type walk over root children — node names carry unit addresses,
  never path-lookup) minus `/memreserve/` + kernel image + DTB blob →
  memmap normalized regions (kernel/mm/memmap.c, pure build fn host-tested);
  bitmap pmm ported from ToyOS64 (pmm.c — Limine memmap → memmap regions,
  HHDM → pmm_phys_to_virt identity seam, pmm_reclaim_bootloader dropped);
  heap provider now pmm-backed (heap_provider.c replaces the R1 static
  arena); bootmmu maps discovered banks ([0, max(4 GiB, ram_top)) — RAM
  blocks Normal, rest Device; L2 pool of 16 tables in .bss). Boot order
  moved: memmap discovery runs PRE-MMU (DTB walk = plain loads, the one
  thing legal on all-Device memory) so bootmmu can map real banks.
  Acceptance: test-host 82/82 (bitmap 30 + memmap 15 new), smoke +
  smoke-tcg green -smp 4 with R2.1 pmm PASS lines, U-Boot/TCG path boots
  with correct reservations (U-Boot relocates the DTB near RAM top).
  Heap contiguity invariant: heap pages never return to pmm mid-life, so
  lowest-free-first keeps heap_grow's extend-in-place working while the
  heap is the only runtime pmm client — revisit when R2.2 page tables
  interleave.
- SMP bring-up shape (smp.c + secondary_entry in entry.S): PSCI CPU_ON
  (fnid 0xC4000003, conduit from DTB /psci method — hvc on QEMU both boot
  paths) releases each AP at EL2 → same settle as _start → per-AP boot
  stack → VBAR → bootmmu_ap_enable (shared identity tables; per-core
  SCTLR/TCR/TTBR) → secondary_main: percpu_init_ap (TPIDR bind only —
  slots are ALL initialised by BSP's percpu_init, sched_init fills .idle,
  so init_ap must never re-run cpu_local_init) → gicv3_init_ap (per-core
  GICR wake + iface) → arm own CNTV → online handshake (release/acquire
  on a bitmap) → idle-thread SP swap mirroring sched_start's tail.
- QEMU GICv3 model quirk: `gicr_ienabler0` resets to 0 (real hardware
  resets SGIs enabled = 0x0000FFFF), so an explicit GICR_ISENABLER0 write
  is REQUIRED or SGIs are never delivered. But under QEMU 11.0.1 HVF,
  writing 0xffff BEFORE the CPU-interface block (PMR/IGRPEN1) trips the
  backend's `assert(isv)` (ISV=0 data-abort emulation, known-broken area;
  TCG unaffected). Working shape on both accels: enable just the SGI ids
  in use, AFTER the interface block. Also: HVF DOES emulate ICC_SGI1R_EL1
  (routed to the shared TCG GIC handler — verify delivery issues against
  the model's reset state before suspecting the accelerator).
- Serial is SMP-safe at two granularities: putchar takes an irqsave lock
  per character (keeps bytes intact), serial_printf formats the whole
  line and drains it under one hold (one call = one intact line — a
  per-char lock alone still interleaves lines mid-way). The lock is armed
  by bootmmu_init (post-MMU); pre-MMU prints are unlocked single-core.
- sched_kick_idle now sends SGI 0 (GICV3_SGI_RESCHED) via ICC_SGI1R_EL1 —
  WFI wake only, the handler is EOI-only and the idle loop re-checks.
- Timer note: the tick is one-shot with deadline += period on reload; if
  the deadline expires while IRQs are masked (boot/bring-up windows), the
  first unmask produces a bounded catch-up burst of ticks. Harmless but
  visible in QEMU traces.
- MMU must go on before the first atomic: with the MMU off all memory is
  Device type and atomics/exclusives are unsupported there (LDAXR faults
  DFSC 0x35 under HVF, LDADD 0x61 under TCG — first hit was heap_lock).
  `bootmmu.c` builds a static identity map (RAM Normal WB / MMIO Device)
  purely to enable caches + atomics; it is R1 scaffolding, NOT the R2 paging
  design (no TTBR1 split / user spaces / ASIDs / CoW). Descriptor encodings
  follow Linux: block/section = 0b01, table/page = 0b11 in bits[1:0] —
  writing 0b10 for block is reserved and faults that level's walk.
- Vector-slot LR trap: a slot's `mov x30, #slot` destroys the interrupted
  context's LR before anything saves it (x86's CPU pushes RIP for you; ARM
  gives nothing). Any preempted leaf function that returns via `ret x30`
  resumes and jumps to the slot number. Slots now stash the original LR
  below the frame first; trap_entry_asm patches it into the frame's x30 slot.
- GICv3 essentials learned: ICC_PMR_EL1 resets to 0 and masks everything —
  set 0xff before ICC_IGRPEN1; GICR access needs WAKER wake first; find this
  core's redistributor by matching GICR_TYPER[63:32] against MPIDR (not frame
  index); EOI only for INTID < 1020; virt timer = PPI 11 → INTID 27; SGIs+PPIs
  must be Group 1 (GICR_IGROUPR0) or they arrive as FIQ, not IRQ. Registered
  handlers own their EOI (a handler may schedule() away and never return
  through gicv3_irq_enter); the dispatcher EOIs only unhandled INTIDs.
- CNTFRQ_EL0 differs per boot path (24 MHz under HVF `-kernel`, 1 GHz under
  TCG/U-Boot) — always read it at runtime, never hardcode.
- clang -O2 merges adjacent 32-bit loads into one 64-bit access: on Device
  memory that alignment-faults (the historical fdt_cell64 lesson; RAM is
  Normal since bootmmu, so this now only bites MMIO paths).
- serial_print_hex already prints the "0x" prefix — don't add a literal one.
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

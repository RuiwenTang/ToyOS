#!/usr/bin/env python3
"""smoke.py — headless serial smoke regression.

Blueprint test layer 3: boots Build/aarch64/kernel/Image under QEMU with
the same machine flags as `pixi run boot`, captures the serial output and
passes once every expected line has appeared. Kills QEMU on success;
dumps the log and fails on timeout or early exit. Finishes in a few
seconds so it stays inside the project's ~1-minute agent smoke rule.

TOYOS_SMOKE_ACCEL=tcg swaps the accelerator (the TCG/U-Boot-path analogue
of the same regression; slower, still seconds).
"""

import os
import select
import subprocess
import sys
import time

ACCEL = os.environ.get("TOYOS_SMOKE_ACCEL", "hvf")

QEMU_CMD = [
    "qemu-system-aarch64",
    "-M", "virt,gic-version=3",
    "-cpu", "max",
    "-accel", ACCEL,
    "-smp", "4",   # R1 acceptance: PSCI bring-up + per-core preemption
    "-m", "1G",
    "-nographic",
    "-kernel", "Build/aarch64/kernel/Image",
]

# Lines that must appear on serial before the deadline. Order is not
# enforced; worker lines interleave arbitrarily — that interleaving IS the
# tick-preemption proof (the workers never yield). Every worker/iter pair
# is listed so a lost switch fails loudly.
EXPECTED = [
    # --- R2.1 acceptance: real physical memory ---
    b"memmap: bank",                     # /memory discovery (base varies)
    b"memmap: kernel image [",           # image + dtb reservations active
    b"[PMM] Initialized successfully",
    b"[PMM] Free pages: ",
    b"R2.1: pmm smoke PASS",
    b"[heap] provider: pmm",
    b"mmu: boot identity map on",
    # --- R1 acceptance (unchanged) ---
    b"R1: returned from SVC, context restore OK",
    b"gic: distributor + redistributor + cpu interface online",
    b"timer: CNTFRQ",
    b"[Timer] tick #1",
    b"[Sched] Boot thread TCB created (tid=0)",
    b"sched: worker 0 iter 1 (tick-preempted)",
    b"sched: worker 0 iter 2 (tick-preempted)",
    b"sched: worker 0 iter 3 (tick-preempted)",
    b"sched: worker 1 iter 1 (tick-preempted)",
    b"sched: worker 1 iter 2 (tick-preempted)",
    b"sched: worker 1 iter 3 (tick-preempted)",
    b"sched: worker 2 iter 1 (tick-preempted)",
    b"sched: worker 2 iter 2 (tick-preempted)",
    b"sched: worker 2 iter 3 (tick-preempted)",
    b"R1: scheduler smoke PASS",
    # --- R1 SMP acceptance: all cores online + IPI echo ---
    b"psci: conduit",  # version digits differ per boot path — prefix only
    b"smp: 4 cpu(s)",
    b"[SMP] core 1 online",
    b"[SMP] core 2 online",
    b"[SMP] core 3 online",
    b"SMP: all 4 cores online",
    b"ipi: round 1",
    b"ipi: round 2",
    b"ipi: round 3",
    b"SMP: IPI echo PASS",
    # Per-core preempt proof: 2 workers x 3 iters on each of cores 1-3.
    b"smp: cpu 1 w0 iter 1 (tick-preempted)",
    b"smp: cpu 1 w0 iter 2 (tick-preempted)",
    b"smp: cpu 1 w0 iter 3 (tick-preempted)",
    b"smp: cpu 1 w1 iter 1 (tick-preempted)",
    b"smp: cpu 1 w1 iter 2 (tick-preempted)",
    b"smp: cpu 1 w1 iter 3 (tick-preempted)",
    b"smp: cpu 2 w0 iter 1 (tick-preempted)",
    b"smp: cpu 2 w0 iter 2 (tick-preempted)",
    b"smp: cpu 2 w0 iter 3 (tick-preempted)",
    b"smp: cpu 2 w1 iter 1 (tick-preempted)",
    b"smp: cpu 2 w1 iter 2 (tick-preempted)",
    b"smp: cpu 2 w1 iter 3 (tick-preempted)",
    b"smp: cpu 3 w0 iter 1 (tick-preempted)",
    b"smp: cpu 3 w0 iter 2 (tick-preempted)",
    b"smp: cpu 3 w0 iter 3 (tick-preempted)",
    b"smp: cpu 3 w1 iter 1 (tick-preempted)",
    b"smp: cpu 3 w1 iter 2 (tick-preempted)",
    b"smp: cpu 3 w1 iter 3 (tick-preempted)",
    b"SMP: per-core preempt smoke PASS",
]

TIMEOUT_SECS = 60


def main() -> int:
    try:
        proc = subprocess.Popen(
            QEMU_CMD,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except FileNotFoundError:
        print("smoke: qemu-system-aarch64 not found (brew install qemu)",
              file=sys.stderr)
        return 1

    log = bytearray()
    deadline = time.monotonic() + TIMEOUT_SECS
    try:
        while any(e not in log for e in EXPECTED):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or proc.poll() is not None:
                break
            ready, _, _ = select.select([proc.stdout], [], [],
                                        min(remaining, 1.0))
            if ready:
                chunk = proc.stdout.read1(4096)
                if not chunk:
                    break
                log.extend(chunk)
    finally:
        proc.kill()
        proc.wait()

    sys.stdout.write(log.decode("utf-8", errors="replace"))
    missing = [e.decode() for e in EXPECTED if e not in log]
    if missing:
        print(f"smoke: FAIL, missing {missing}", file=sys.stderr)
        return 1
    print("smoke: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

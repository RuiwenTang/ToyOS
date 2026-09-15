#!/usr/bin/env python3
"""smoke.py — headless serial smoke regression.

Blueprint test layer 3: boots Build/aarch64/kernel/Image under QEMU with
the same machine flags as `pixi run boot`, captures the serial output and
passes once every expected line has appeared. Kills QEMU on success;
dumps the log and fails on timeout or early exit. Finishes in a few
seconds so it stays inside the project's ~1-minute agent smoke rule.
"""

import select
import subprocess
import sys
import time

QEMU_CMD = [
    "qemu-system-aarch64",
    "-M", "virt,gic-version=3",
    "-cpu", "max",
    "-accel", "hvf",
    "-m", "1G",
    "-nographic",
    "-kernel", "Build/aarch64/kernel/Image",
]

# Lines that must appear on serial before the deadline. Order is not
# enforced; tick lines interleave with the boot dump. The park line last
# keeps the captured log complete through the measured-rate summary.
EXPECTED = [
    b"R1: returned from SVC, context restore OK",
    b"gic: distributor + redistributor + cpu interface online",
    b"timer: CNTFRQ",
    b"timer: tick 30 (irq context)",
    b"R1: gicv3+timer smoke PASS",
    b"R1 slice complete (scheduler + PSCI next), parking",
]

TIMEOUT_SECS = 30


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

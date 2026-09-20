/*
 * psci.h — PSCI firmware calls (aarch64)
 *
 * Secondary cores on both targets stay powered off until firmware turns
 * them on: QEMU's virtual PSCI (HVC behind its emulated EL) and the R5C's
 * TF-A/BL31 (SMC at EL3). The conduit comes from the DTB /psci node's
 * `method` property — never hard-coded, matching every other base address
 * discovery. x86 has no analogue; this replaces the INIT-SIPI-SIPI dance.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_PSCI_H
#define TOYOS_ARCH_AARCH64_PSCI_H

#include <toyos/kernel/types.h>

/* PSCI 0.2+ function IDs. PSCI_VERSION is SMC32-only per the spec; CPU_ON
 * from an AArch64 caller must use the 64-bit form (the conduit — HVC vs
 * SMC — does not change the IDs). */
#define PSCI_FN_VERSION 0x84000000u
#define PSCI_FN_CPU_ON_64 0xc4000003u

/* Return codes (negative; success is 0). */
#define PSCI_SUCCESS 0
#define PSCI_NOT_SUPPORTED (-1)
#define PSCI_INVALID_PARAMS (-2)
#define PSCI_DENIED (-3)
#define PSCI_ALREADY_ON (-4)
#define PSCI_ON_PENDING (-5)
#define PSCI_INTERNAL_FAILURE (-6)

/*
 * psci_init - Discover the conduit from the DTB /psci node and verify the
 *             firmware with a PSCI_VERSION round-trip. Returns 0 if CPU_ON
 *             is usable, -1 otherwise (loudly — bring-up then degrades to
 *             single core).
 */
int psci_init(const void* dtb);

/*
 * psci_cpu_on - Power on the core whose MPIDR affinity is @target_cpu and
 *               start it at @entry with x0 = @context_id.
 * Returns a PSCI return code (0 = PSCI_SUCCESS).
 */
int32_t psci_cpu_on(uint64_t target_cpu, uintptr_t entry, uint64_t context_id);

#endif /* TOYOS_ARCH_AARCH64_PSCI_H */

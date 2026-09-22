/*
 * psci.c — PSCI firmware calls (aarch64)
 *
 * The conduit is fixed by psci_init (DTB /psci `method`) into one of two
 * four-register call shims; both leave x0 as the return code, the PSCI
 * register contract. Everything runs at EL1: the HVC/SMC traps out to the
 * firmware's exception level (QEMU's emulated EL, TF-A's EL3 on the R5C)
 * and never touches our own vectors.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/psci.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/string.h>
#include <toyos/kernel/types.h>

enum psci_conduit { PSCI_CONDUIT_NONE, PSCI_CONDUIT_HVC, PSCI_CONDUIT_SMC };

static enum psci_conduit conduit = PSCI_CONDUIT_NONE;

static int32_t psci_call(uint64_t fid, uint64_t x1, uint64_t x2, uint64_t x3) {
  register uint64_t r0 __asm__("x0") = fid;
  register uint64_t r1 __asm__("x1") = x1;
  register uint64_t r2 __asm__("x2") = x2;
  register uint64_t r3 __asm__("x3") = x3;

  if (conduit == PSCI_CONDUIT_HVC) {
    __asm__ volatile("hvc #0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3)
                     : "memory");
  } else {
    __asm__ volatile("smc #0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3)
                     : "memory");
  }
  return (int32_t)r0;
}

int psci_init(const void* dtb) {
  fdt_node_t node = fdt_find_node(dtb, "/psci");
  if (node < 0) node = fdt_find_compatible(dtb, "arm,psci-0.2");
  if (node < 0) {
    serial_puts("psci: no /psci node — secondaries unreachable\n");
    return -1;
  }

  const void* method;
  int len = fdt_get_prop(dtb, node, "method", &method);
  if (len >= 4 && strcmp((const char*)method, "hvc") == 0) {
    conduit = PSCI_CONDUIT_HVC;
  } else if (len >= 4 && strcmp((const char*)method, "smc") == 0) {
    conduit = PSCI_CONDUIT_SMC;
  } else {
    serial_puts("psci: /psci has no usable method property\n");
    return -1;
  }

  /* Round-trip PSCI_VERSION before trusting the conduit with CPU_ON. */
  int32_t ver = psci_call(PSCI_FN_VERSION, 0, 0, 0);
  serial_printf("psci: conduit %s, version %x\n",
                conduit == PSCI_CONDUIT_HVC ? "hvc" : "smc",
                (uint64_t)(uint32_t)ver);

  if (ver < 0) {
    serial_puts("psci: firmware reports NOT_SUPPORTED — staying single core\n");
    conduit = PSCI_CONDUIT_NONE;
    return -1;
  }
  return 0;
}

int32_t psci_cpu_on(uint64_t target_cpu, uintptr_t entry, uint64_t context_id) {
  if (conduit == PSCI_CONDUIT_NONE) return PSCI_NOT_SUPPORTED;
  return psci_call(PSCI_FN_CPU_ON_64, target_cpu, (uint64_t)entry, context_id);
}

#include <lai/core.h>
#include <lai/helpers/pc-bios.h>
#include <stddef.h>
#include <string.h>
#include <umm_malloc.h>

#include "kprintf.h"
#include "x86/io.h"

// lai host function stub

void* laihost_malloc(size_t size) { return umm_malloc(size); }

void* laihost_realloc(void* ptr, size_t newsize, size_t oldsize) {
  (void)oldsize;
  return umm_realloc(ptr, newsize);
}

void* laihost_map(size_t address, size_t count) {
  // need to map this address if not present by kernel
  (void)address;
  (void)count;
  return (void*)address;
}

void laihost_unmap(void* ptr, size_t count) {
  (void)ptr;
  (void)count;
}

void laihost_free(void* ptr, size_t size) {
  (void)size;
  umm_free(ptr);
}

void laihost_outb(uint16_t port, uint8_t value) { x86_out8(port, value); }

void laihost_outw(uint16_t port, uint16_t value) { x86_out16(port, value); }

void laihost_outd(uint16_t port, uint32_t value) { x86_out32(port, value); }

uint8_t laihost_inb(uint16_t port) { return x86_in8(port); }

uint16_t laihost_inw(uint16_t port) { return x86_in16(port); }

uint32_t laihost_ind(uint16_t port) { return x86_in32(port); }

void laihost_sleep(uint64_t ms) { (void)ms; }

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun,
                        uint16_t offset, uint16_t val) {
  (void)seg;
  (void)bus;
  (void)slot;
  (void)fun;
  (void)offset;
  (void)val;
}

// Returns the (virtual) address of the n-th table that has the given signature,
// or NULL when no such table was found.
void* laihost_scan(const char* sig, size_t index) {
  // detect rsdp using lai helper function
  struct lai_rsdp_info info;

  lai_api_error_t ret = lai_bios_detect_rsdp(&info);

  if (ret != LAI_ERROR_NONE) {
    return 0;
  }

  kprintf("Find rsdp at 0x%x | with verson %d\n", info.rsdp_address,
          info.acpi_version);

  acpi_rsdt_t* rsdt = (acpi_rsdt_t*)info.rsdt_address;

  int num_ptrs =
      (rsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint32_t);

  if (memcmp(sig, "DSDT", 4) == 0) {
    acpi_fadt_t* fadt = (acpi_fadt_t*)rsdt->tables[0];

    return (void*)fadt->dsdt;
  }

  for (int i = 0; i < num_ptrs; i++) {
    acpi_header_t* entry = (acpi_header_t*)rsdt->tables[i];

    if (memcmp(entry->signature, sig, 4) == 0) {
      return entry;
    }
  }

  return 0;
}
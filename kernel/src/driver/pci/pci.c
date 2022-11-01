#include <driver/pci/ide.h>
#include <driver/pci/pci.h>

#include "kprintf.h"
#include "x86/io.h"

/*
 * There can be up to 256 PCI busses, but it takes a noticeable
 * amount of time to scan that whole space. Limit the number of
 * supported busses to something more reasonable...
 */
#define PCI_MAX_BUSSES 0x20

#define PCI_REG_CONFIG_ADDRESS 0xCF8
#define PCI_REG_CONFIG_DATA 0xCFC

static uint32_t pci_config_pack_address(const PCIAddress* addr,
                                        uint16_t offset) {
  const uint32_t enable_bit = 0x80000000UL;

  return (((uint32_t)addr->bus << 16) | ((uint32_t)addr->device << 11) |
          ((uint32_t)addr->function << 8) | offset | enable_bit);
}

uint32_t pci_config_read32(const PCIAddress* addr, uint16_t offset) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  return x86_in32(PCI_REG_CONFIG_DATA);
}

uint16_t pci_config_read16(const PCIAddress* addr, uint16_t offset) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  return x86_in16(PCI_REG_CONFIG_DATA);
}

uint8_t pci_config_read8(const PCIAddress* addr, uint16_t offset) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  return x86_in8(PCI_REG_CONFIG_DATA);
}

void pci_config_write32(const PCIAddress* addr, uint16_t offset,
                        uint32_t data) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  x86_out32(PCI_REG_CONFIG_DATA, data);
}

void pci_config_write16(const PCIAddress* addr, uint16_t offset,
                        uint16_t data) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  x86_out16(PCI_REG_CONFIG_DATA, data);
}

void pci_config_write8(const PCIAddress* addr, uint16_t offset, uint8_t data) {
  x86_out32(PCI_REG_CONFIG_ADDRESS, pci_config_pack_address(addr, offset));
  x86_out8(PCI_REG_CONFIG_DATA, data);
}

int pci_scanbus(PCIScanState* state) {
  PCIConfigSpace config;
  for (;;) {
    config.words[0] = pci_config_read32(&state->next_addr, 0);
    state->addr = state->next_addr;
    if (++state->next_addr.function == 0x8) {
      state->next_addr.function = 0;
      if (++state->next_addr.device == 0x20) {
        state->next_addr.device = 0;
        if (++state->next_addr.bus == PCI_MAX_BUSSES) {
          return 0;
        }
      }
    }

    if (config.words[0] != 0xFFFFFFFFUL) {
      state->vendor_id = config.vendorId;
      state->device_id = config.device_id;
      return 1;
    }
  }
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id,
                    PCIAddress* addr_out) {
  PCIScanState bus_scan = {};

  while (pci_scanbus(&bus_scan)) {
    if (bus_scan.vendor_id == vendor_id && bus_scan.device_id == device_id) {
      *addr_out = bus_scan.addr;
      return 1;
    }
  }
  return 0;
}

void pci_set_BAR(const PCIAddress* addr, int index, uint32_t value) {
  pci_config_write32(addr, offsetof(PCIConfigSpace, BAR[index]), value);
}

uint32_t pci_get_BAR(const PCIAddress* addr, int index) {
  uint32_t bar = pci_config_read32(addr, offsetof(PCIConfigSpace, BAR[index]));

  uint32_t mask = (bar & PCI_CONF_BAR_IO) ? 0x3 : 0xf;
  return bar & ~mask;
}

void pci_set_mem_enable(const PCIAddress* addr, int enable) {
  uint16_t command = pci_config_read16(addr, offsetof(PCIConfigSpace, command));

  /* Mem space enable, IO space enable, bus mastering. */
  const uint16_t flags = 0x0007;

  if (enable) {
    command |= flags;
  } else {
    command &= ~flags;
  }

  pci_config_write16(addr, offsetof(PCIConfigSpace, command), command);
}

void pci_init() {
  // parallel IDE
  ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
}

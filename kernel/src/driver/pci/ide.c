#include <driver/pci/ide.h>

#include "kprintf.h"
#include "mmu/heap.h"
#include "x86/io.h"

IDEChannelRegisters g_channels[2];
IDEDevice g_devices[4];

// 512 for all devices
uint8_t* g_ide_buffer = NULL;

static void insl(uint16_t port, uint32_t* buffer, uint32_t times) {
  for (uint32_t i = 0; i < times; i++) {
    buffer[i] = x86_in32(port);
  }
}

static void sleep(uint32_t time) {
  for (uint32_t i = 0; i < time * 100; i++) {
    x86_iowait();
  }
}

uint8_t ide_read(uint8_t channel, uint8_t reg) {
  uint8_t result = 0;
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, 0x80 | g_channels[channel].nIEN);
  if (reg < 0x08)
    result = x86_in8(g_channels[channel].base + reg - 0x00);
  else if (reg < 0x0C)
    result = x86_in8(g_channels[channel].base + reg - 0x06);
  else if (reg < 0x0E)
    result = x86_in8(g_channels[channel].ctrl + reg - 0x0A);
  else if (reg < 0x16)
    result = x86_in8(g_channels[channel].bmide + reg - 0x0E);
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, g_channels[channel].nIEN);

  return result;
}

void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
  if (reg > 0x7 && reg < 0xc) {
    ide_write(channel, ATA_REG_CONTROL, 0x80 | g_channels[channel].nIEN);
  }

  if (reg < 0x8) {
    x86_out8(g_channels[channel].base + reg - 0x00, data);
  } else if (reg < 0xc) {
    x86_out8(g_channels[channel].base + reg - 0x06, data);
  } else if (reg < 0xe) {
    x86_out8(g_channels[channel].ctrl + reg - 0x0A, data);
  } else if (reg < 0x16) {
    x86_out8(g_channels[channel].bmide + reg - 0x0E, data);
  }

  if (reg > 0x7 && reg < 0xc) {
    ide_write(channel, ATA_REG_CONTROL, g_channels[channel].nIEN);
  }
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t* buffer,
                     uint32_t quads) {
  /* WARNING: This code contains a serious bug. The inline assembly trashes ES
   * and ESP for all of the code the compiler generates between the inline
   *           assembly blocks.
   */
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, 0x80 | g_channels[channel].nIEN);
  if (reg < 0x08)
    insl(g_channels[channel].base + reg - 0x00, buffer, quads);
  else if (reg < 0x0C)
    insl(g_channels[channel].base + reg - 0x06, buffer, quads);
  else if (reg < 0x0E)
    insl(g_channels[channel].ctrl + reg - 0x0A, buffer, quads);
  else if (reg < 0x16)
    insl(g_channels[channel].bmide + reg - 0x0E, buffer, quads);
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, g_channels[channel].nIEN);
}

uint8_t ide_polling(uint8_t channel, uint32_t advanced_check) {
  // (I) Delay 400 nanosecond for BSY to be set:
  // -------------------------------------------------
  for (int i = 0; i < 4; i++) {
    ide_read(channel, ATA_REG_ALTSTATUS);  // Reading the Alternate Status port
                                           // wastes 100ns; loop four times.
  }

  // (II) Wait for BSY to be cleared:
  // -------------------------------------------------
  while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
    ;  // Wait for BSY to be zero.

  if (advanced_check) {
    uint8_t state = ide_read(channel, ATA_REG_STATUS);  // Read Status Register.

    // (III) Check For Errors:
    // -------------------------------------------------
    if (state & ATA_SR_ERR) return 2;  // Error.

    // (IV) Check If Device fault:
    // -------------------------------------------------
    if (state & ATA_SR_DF) return 1;  // Device Fault.

    // (V) Check DRQ:
    // -------------------------------------------------
    // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
    if ((state & ATA_SR_DRQ) == 0) return 3;  // DRQ should be set
  }

  return 0;  // No Error.
}

void ide_initialize(uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3,
                    uint32_t bar4) {
  g_ide_buffer = kmalloc(512);

  g_channels[ATA_PRIMARY].base = (bar0 & 0xFFFFFFFC) + 0x1F0 * (!bar0);
  g_channels[ATA_PRIMARY].ctrl = (bar1 & 0xFFFFFFFC) + 0x3F6 * (!bar1);
  g_channels[ATA_SECONDARY].base = (bar2 & 0xFFFFFFFC) + 0x170 * (!bar2);
  g_channels[ATA_SECONDARY].ctrl = (bar3 & 0xFFFFFFFC) + 0x376 * (!bar3);
  g_channels[ATA_PRIMARY].bmide = (bar4 & 0xFFFFFFFC) + 0;    // bus master ide
  g_channels[ATA_SECONDARY].bmide = (bar4 & 0xFFFFFFFC) + 8;  // bus master ide

  ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
  ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

  int32_t count = 0;

  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 2; j++) {
      uint8_t err = 0;
      uint8_t type = IDE_ATA;
      uint8_t status = 0;

      g_devices[count].reserved = 0;  // Assuming that no drive here.

      // (I) Select Drive:
      ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));  // Select Drive.
      // Fixme wait a moment
      sleep(1);

      // (II) Send ATA Identify Command:
      ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
      sleep(1);  // This function should be implemented in your OS. which waits
                 // for 1 ms. it is based on System Timer Device Driver.

      // (III) Polling:
      if (ide_read(i, ATA_REG_STATUS) == 0) {
        continue;  // If Status = 0, No Device.
      }

      while (1) {
        status = ide_read(i, ATA_REG_STATUS);
        if ((status & ATA_SR_ERR)) {
          err = 1;
          break;
        }  // If Err, Device is not ATA.
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
          break;  // Everything is right.
        }
      }

      // (IV) Probe for ATAPI Devices:
      if (err != 0) {
        uint8_t cl = ide_read(i, ATA_REG_LBA1);
        uint8_t ch = ide_read(i, ATA_REG_LBA2);

        if (cl == 0x14 && ch == 0xEB)
          type = IDE_ATAPI;
        else if (cl == 0x69 && ch == 0x96)
          type = IDE_ATAPI;
        else
          continue;  // Unknown Type (may not be a device).

        ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
        sleep(1);
      }

      // (V) Read Identification Space of the Device:
      ide_read_buffer(i, ATA_REG_DATA, (uint32_t*)g_ide_buffer, 128);

      // (VI) Read Device Parameters:
      g_devices[count].reserved = 1;
      g_devices[count].type = type;
      g_devices[count].channel = i;
      g_devices[count].drive = j;
      g_devices[count].signature =
          *((uint16_t*)(g_ide_buffer + ATA_IDENT_DEVICETYPE));
      g_devices[count].capabilities =
          *((uint16_t*)(g_ide_buffer + ATA_IDENT_CAPABILITIES));
      g_devices[count].command_sets =
          *((uint32_t*)(g_ide_buffer + ATA_IDENT_COMMANDSETS));

      // (VII) Get Size:
      if (g_devices[count].command_sets & (1 << 26)) {
        // device uses 48 - bit addressing:
        g_devices[count].size =
            *((uint32_t*)(g_ide_buffer + ATA_IDENT_MAX_LBA_EXT));
      } else {
        // device uses chs or 28-bit addressing:
        g_devices[count].size =
            *((uint32_t*)(g_ide_buffer + ATA_IDENT_MAX_LBA));
      }

      // (VIII) String indicates model of device (like Western Digital HDD and
      // SONY DVD-RW...):
      for (uint32_t k = 0; k < 40; k += 2) {
        g_devices[count].model[k] = g_ide_buffer[ATA_IDENT_MODEL + k + 1];
        g_devices[count].model[k + 1] = g_ide_buffer[ATA_IDENT_MODEL + k];
      }
      g_devices[count].model[40] = 0;

      count++;
    }
  }

  for (uint32_t i = 0; i < 4; i++) {
    if (g_devices[i].reserved == 1) {
      const char* ata_type[] = {
          "ATA",
          "ATAPI",
      };
      kprintf(" Found %s Driver %dMB - %s\n", ata_type[g_devices[i].type],
              g_devices[i].size / 1024 / 2, g_devices[i].model);
    }
  }
}
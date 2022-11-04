#include <diskio.h>
#include <driver/pci/ide.h>
#include <ff.h>
#include <stddef.h>

IDEDevice *m_devices = NULL;
uint32_t m_devices_index = -1;

DSTATUS RAM_disk_initialize() {
  IDEDevice *ide_devices = ide_get_devices();
  for (uint32_t i = 0; i < 4; i++) {
    IDEDevice *device = ide_devices + i;
    if (device->reserved) {
      m_devices = device;
      m_devices_index = i;
      return 0;
    }
  }
  return STA_NODISK;
}

DSTATUS RAM_disk_read(BYTE *buf, LBA_t sector, UINT count) {
  if (m_devices == NULL || m_devices_index == -1) {
    return STA_NOINIT;
  }

  if (ide_ata_access(0, m_devices_index, sector, count, buf)) {
    return STA_NODISK;
  }

  return 0;
}

DSTATUS RAM_disk_write(const BYTE *buf, LBA_t sector, UINT count) {
  if (m_devices == NULL || m_devices_index == -1) {
    return STA_NOINIT;
  }

  if (ide_ata_access(1, m_devices_index, sector, count, (BYTE *)buf)) {
    return STA_NODISK;
  }

  return 0;
}

DSTATUS RAM_disk_ioctl(BYTE cmd, void *buff) {
  if (cmd == GET_SECTOR_COUNT) {
    return m_devices->size;
  }

  return STA_NOINIT;
}
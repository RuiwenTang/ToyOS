#ifndef BOOT_X86_BIOS_H
#define BOOT_X86_BIOS_H

#include <stdint.h>

// just a mark
#define ASMFUNC

/**
 * @brief               This function switch back to real mode, and use bios
 *                      int 0x13 read one sector and switch mack to protect
 *                      mode.
 *                      To keep function simple, this function only read one
 *                      sector. And this may be slow.
 *
 *
 * @param drive         dirver number of boot disk, this is passed by bios
 * @param lba_start     lba start location
 * @param data_out      output address
 * @return int          0 success, other value means failed
 */
int ASMFUNC bios_disk_read(uint8_t drive, uint32_t lba_start, void *data_out);

#endif // BOOT_X86_BIOS_H

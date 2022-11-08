#include <elf/elf.h>
#include <ff.h>

#include "mmu/heap.h"

// backend impl

typedef struct {
  Elf32_File base;
  FIL* ff_file;
} SYSElf_File;

static int sys_elf_seek(Elf32_File* file, uint32_t offset) {
  SYSElf_File* elf_file = (SYSElf_File*)file;
  if (elf_file->ff_file == NULL) {
    return 1;
  }

  FRESULT res = f_lseek(elf_file->ff_file, offset);

  if (res != FR_OK) {
    return 2;
  }

  return 0;
}

static int sys_elf_read(Elf32_File* file, char* buf, uint32_t size) {
  SYSElf_File* elf_file = (SYSElf_File*)file;
  if (elf_file->ff_file == NULL) {
    return 1;
  }

  UINT read_size = 0;
  FRESULT res = f_read(elf_file->ff_file, buf, size, &read_size);

  if (res != FR_OK || read_size == 0) {
    return 2;
  }

  return 0;
}

Elf32_File* elf_open_file(const char* path) {
  FIL* ff_file = (FIL*)kmalloc(sizeof(FIL));

  FRESULT res = f_open(ff_file, path, FA_READ | FA_OPEN_EXISTING);

  if (res != FR_OK) {
    kfree(ff_file);
    return NULL;
  }

  SYSElf_File* elf_file = (SYSElf_File*)kmalloc(sizeof(SYSElf_File));

  elf_file->ff_file = ff_file;

  elf_file->base.impl_seek = sys_elf_seek;
  elf_file->base.impl_read = sys_elf_read;

  return (Elf32_File*)elf_file;
}

int elf_close_file(Elf32_File* file) {
  SYSElf_File* elf_file = (SYSElf_File*)file;

  f_close(elf_file->ff_file);

  kfree(elf_file->ff_file);
  kfree(elf_file);

  return 0;
}

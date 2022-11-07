
#include <elf/elf.h>

int elf_check_valid(Elf32_File *file) {
  file->impl_seek(file, 0);

  file->impl_read(file, (char *)&file->header, sizeof(Elf32_Ehdr));

  if (file->header.e_ident[EI_MAG0] != ELFMAG0) {
    // ELF Header EI_MAG0 incorrect
    return 1;
  }

  if (file->header.e_ident[EI_MAG1] != ELFMAG1) {
    // ELF Header EI_MAG1 incorrect
    return 2;
  }

  if (file->header.e_ident[EI_MAG2] != ELFMAG2) {
    // ELF Header EI_MAG2 incorrect
    return 3;
  }

  if (file->header.e_ident[EI_MAG3] != ELFMAG3) {
    // ELF Header EI_MAG3 incorrect
    return 4;
  }

  if (file->header.e_ident[EI_CLASS] != ELFCLASS32) {
    // for now only support x86
    return 5;
  }

  if (file->header.e_ident[EI_DATA] != ELFDATA2LSB) {
    // only support Little endian
    return 6;
  }

  if (file->header.e_machine != EM_386) {
    // only support x86 architecture
    return 7;
  }

  if (file->header.e_type != ET_EXEC && file->header.e_type != ET_SHARED) {
    // unsupport elf type
    return 8;
  }

  return 0;
}
#include "elf.h"
#include "printf.h"
#include <stdbool.h>
#include <stddef.h>

static void elf_memcpy(uint8_t *dst, uint8_t *str, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    dst[i] = str[i];
  }
}

static bool elf_check_file(Elf32_Ehdr *hdr) {
  if (!hdr)
    return false;
  if (hdr->e_ident[EI_MAG0] != ELFMAG0) {
    printf("ELF Header EI_MAG0 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG1] != ELFMAG1) {
    printf("ELF Header EI_MAG1 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG2] != ELFMAG2) {
    printf("ELF Header EI_MAG2 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG3] != ELFMAG3) {
    printf("ELF Header EI_MAG3 incorrect.\n");
    return false;
  }
  return true;
}

static bool elf_check_supported(Elf32_Ehdr *hdr) {
  if (!elf_check_file(hdr)) {
    printf("Invalid ELF File.\n");
    return false;
  }
  if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
    printf("Unsupported ELF File Class.\n");
    return false;
  }
  if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
    printf("Unsupported ELF File byte order.\n");
    return false;
  }
  if (hdr->e_machine != EM_386) {
    printf("Unsupported ELF File target.\n");
    return false;
  }
  if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
    printf("Unsupported ELF File version.\n");
    return false;
  }
  if (hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {
    printf("Unsupported ELF File type.\n");
    return false;
  }
  return true;
}

static inline Elf32_Shdr *elf_sheader(Elf32_Ehdr *hdr) {
  return (Elf32_Shdr *)((int)hdr + hdr->e_shoff);
}

static inline Elf32_Shdr *elf_section(Elf32_Ehdr *hdr, int idx) {
  return &elf_sheader(hdr)[idx];
}

static inline char *elf_str_table(Elf32_Ehdr *hdr) {
  if (hdr->e_shstrndx == SHN_UNDEF)
    return NULL;
  return (char *)hdr + elf_section(hdr, hdr->e_shstrndx)->sh_offset;
}

static inline char *elf_lookup_string(Elf32_Ehdr *hdr, int offset) {
  char *strtab = elf_str_table(hdr);
  if (strtab == NULL)
    return NULL;
  return strtab + offset;
}

static int elf_load_stage1(Elf32_Ehdr *hdr) {
  Elf32_Shdr *shdr = elf_sheader(hdr);

  unsigned int i;
  // Iterate over section headers
  for (i = 0; i < hdr->e_shnum; i++) {
    Elf32_Shdr *section = &shdr[i];
    printf("section addr: %x | offset: %x | name: %s\n", section->sh_addr,
           section->sh_offset, elf_lookup_string(hdr, section->sh_name));
    // If the section isn't present in the file
    if (section->sh_type == SHT_NOBITS) {
      // Skip if it the section is empty
      if (!section->sh_size)
        continue;
      // If the section should appear in memory
      if (section->sh_flags & SHF_ALLOC) {
        printf("section offset: %x, ", section->sh_offset);
      }
    }
  }
  return 0;
}

static int elf_load_stage2(Elf32_Ehdr *hdr) {

  printf("program header number: %d\n", hdr->e_phnum);

  Elf32_Phdr *p_hdr = ((uint8_t *)hdr) + hdr->e_phoff;

  for (uint32_t i = 0; i < hdr->e_phnum; i++) {
    printf("program paddr : %x | file size: %x | file offset: %x | mem size: "
           "%x | type: %d \n",
           p_hdr[i].p_paddr, p_hdr[i].p_filesz, p_hdr[i].p_offset,
           p_hdr[i].p_memsz, p_hdr[i].p_type);
    elf_memcpy((void *)p_hdr[i].p_paddr, ((uint8_t *)hdr) + p_hdr[i].p_offset,
               p_hdr[i].p_filesz);
  }

  return 0;
}

uint32_t elf_load(void *file) {
  if (!elf_check_file(file)) {
    return 0;
  }

  if (!elf_check_supported(file)) {
    return 0;
  }

  Elf32_Ehdr *hdr = (Elf32_Ehdr *)file;

  if (hdr->e_type != ET_EXEC) {
    printf("not executable kernel file \n");
    return 0;
  }

  elf_load_stage1(hdr);

  elf_load_stage2(hdr);

  return hdr->e_entry;
}

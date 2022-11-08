#include "src/elf/elf_loader.h"

#include <elf/elf.h>

#include "kprintf.h"
#include "mmu/heap.h"

#define ELF_DEBUG 1

int load_and_exec(const char* path) {
  Elf32_File* elf_file = elf_open_file(path);

  if (elf_file == NULL) {
#ifdef ELF_DEBUG
    kprintf("Can not open executable at: %s \n", path);
#endif
    return 1;
  }

  if (elf_check_valid(elf_file) != 0) {
#ifdef ELF_DEBUG
    kprintf("file: %s is not executable \n", path);
#endif
    elf_close_file(elf_file);
    return 2;
  }

  uint32_t ph_count = 0;

  elf_enum_phdr(elf_file, NULL, &ph_count);
  if (ph_count == 0) {
#ifdef ELF_DEBUG
    kprintf("file: %s contaons no program headers\n");
#endif
    elf_close_file(elf_file);
    return 3;
  }
  Elf32_Phdr* p_headers = (Elf32_Phdr*)kmalloc(ph_count * sizeof(Elf32_Phdr));
  elf_enum_phdr(elf_file, p_headers, &ph_count);

  uint32_t total_size = p_headers[ph_count - 1].p_vaddr +
                        p_headers[ph_count - 1].p_memsz - p_headers[0].p_vaddr;

#ifdef ELF_DEBUG
  kprintf("found %d p_headers total memsize: %x \n", ph_count, total_size);
#endif

  // clean up
  kfree(p_headers);
  elf_close_file(elf_file);

  return 0;
}
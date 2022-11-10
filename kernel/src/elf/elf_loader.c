#include "src/elf/elf_loader.h"

#include <elf/elf.h>
#include <string.h>

#include "kprintf.h"
#include "mmu/heap.h"
#include "proc/proc.h"
#include "x86/gdt.h"

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

  Proc* proc = init_proc(total_size);

  // prepare proc regs
  proc->regs.cs = USER_CODE_SELECTOR;
  proc->regs.ds = USER_DATA_SELECTOR;
  proc->regs.es = USER_DATA_SELECTOR;
  proc->regs.fs = USER_DATA_SELECTOR;
  proc->regs.ss = USER_DATA_SELECTOR;
  proc->regs.gs = USER_DATA_SELECTOR;
  proc->regs.eip = elf_file->header.e_entry;
  proc->regs.esp = proc->stack_top;
  proc->regs.eflags = 0x1202;

  // copy app code and data
  for (uint32_t i = 0; i < ph_count; i++) {
    uint32_t p_addr = proc_phy_address(proc, p_headers[i].p_vaddr);

    if (p_headers[i].p_filesz == 0) {
      memset((void*)p_addr, 0, p_headers[i].p_memsz);
      continue;
    }

    elf_file->impl_seek(elf_file, p_headers[i].p_offset);
    elf_file->impl_read(elf_file, (char*)p_addr, p_headers[i].p_filesz);

    if (p_headers[i].p_filesz < p_headers[i].p_memsz) {
      memset((void*)(p_addr + p_headers[i].p_filesz), 0,
             p_headers[i].p_memsz - p_headers[i].p_filesz);
    }
  }

  // clean up
  kfree(p_headers);
  elf_close_file(elf_file);

  switch_to_ready(proc);

  return 0;
}
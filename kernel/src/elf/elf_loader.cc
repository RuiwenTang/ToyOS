#include "src/elf/elf_loader.hpp"

#include <string.h>

#include "elf/elf_file_impl.hpp"
#include "kprintf.h"
#include "mmu/heap.h"
#include "proc/proc.h"
#include "x86/gdt.h"

#define ELF_DEBUG 1

int load_and_exec(const char* path) {
  ElfFileImpl impl;

  if (!impl.Open(path)) {
#ifdef ELF_DEBUG
    kprintf("Can not open executable at: %s \n", path);
#endif
    return 1;
  }

  if (!impl.IsValid()) {
#ifdef ELF_DEBUG
    kprintf("file: %s is not executable \n", path);
#endif
    return 2;
  }

  uint32_t ph_count = 0;

  impl.EnumPhdr(nullptr, &ph_count);
  if (ph_count == 0) {
#ifdef ELF_DEBUG
    kprintf("file: %s contaons no program headers\n");
#endif
    return 3;
  }
  Elf32_Phdr* p_headers = (Elf32_Phdr*)kmalloc(ph_count * sizeof(Elf32_Phdr));

  impl.EnumPhdr(p_headers, &ph_count);

  uint32_t total_size = p_headers[ph_count - 1].p_vaddr +
                        p_headers[ph_count - 1].p_memsz - p_headers[0].p_vaddr;

#ifdef ELF_DEBUG
  kprintf("found %d p_headers total memsize: %x \n", ph_count, total_size);
#endif

  Proc* proc = init_proc(total_size);

  // prepare proc regs
  proc_get_stackframe(proc)->cs = USER_CODE_SELECTOR;
  proc_get_stackframe(proc)->ds = USER_DATA_SELECTOR;
  proc_get_stackframe(proc)->es = USER_DATA_SELECTOR;
  proc_get_stackframe(proc)->fs = USER_DATA_SELECTOR;
  proc_get_stackframe(proc)->ss = USER_DATA_SELECTOR;
  proc_get_stackframe(proc)->gs = USER_DATA_SELECTOR;
  proc_get_stackframe(proc)->eip = impl.GetEntryPoint();
  proc_get_stackframe(proc)->esp = proc_get_stacktop(proc);
  proc_get_stackframe(proc)->eflags = 0x1202;

  // copy app code and data
  for (uint32_t i = 0; i < ph_count; i++) {
    uint32_t p_addr = proc_phy_address(proc, p_headers[i].p_vaddr);

    if (p_headers[i].p_filesz == 0) {
      memset((void*)p_addr, 0, p_headers[i].p_memsz);
      continue;
    }

    impl.Seek(p_headers[i].p_offset);
    impl.Read((char*)p_addr, p_headers[i].p_filesz);

    if (p_headers[i].p_filesz < p_headers[i].p_memsz) {
      memset((void*)(p_addr + p_headers[i].p_filesz), 0,
             p_headers[i].p_memsz - p_headers[i].p_filesz);
    }
  }

  // clean up
  kfree(p_headers);

  impl.Close();

  switch_to_ready(proc);
  proc_switch();

  return 0;
}
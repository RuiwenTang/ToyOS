
#include <boot/multiboot.h>
#include <driver/pci/ide.h>
#include <driver/pci/pci.h>

#include "elf/elf_loader.hpp"
#include "fs/vfs.h"
#include "kprintf.h"
#include "mmu/heap.h"
#include "mmu/page.h"
#include "proc/proc.h"
#include "screen/screen.h"
#include "syscall/syscall.h"
#include "x86/gdt.h"
#include "x86/idt.h"
#include "x86/io.h"
#include "x86/irq.h"
#include "x86/timer.h"

uint8_t temp_stack[1024];

void system_init(multiboot_info_t* info, uint32_t stack) {
  gdt_install(stack);
  page_init(info);
  idt_intall();
  irq_install();
  timer_init();
  sys_call_init();
  pci_init();

  vfs_init();
}

void task_a() {
  kprintf("^");

  uint32_t j = 0;
  while (1) {
    kprintf("%d", j);
    j++;
    for (uint32_t i = 0; i < 100000000; i++)
      ;
  }
}

extern "C" uint32_t kernel_main(uint32_t esp, uint32_t eax, uint32_t ebx) {
  if (eax != 0x2BADB002) {
    // eax must be this magic number.
    // other value means boot failed
    return 0x1111;
  }

  multiboot_info_t* mb_info = (multiboot_info_t*)ebx;

  if (mb_info->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
    // no framebuffer video mode
    return 0x1111;
  }

  screen_init(mb_info);
  kprintf("kernel begins... \n");

  system_init(mb_info, esp);

  x86_enable_interrupt();

  while (1)
    ;

  return 0;
}

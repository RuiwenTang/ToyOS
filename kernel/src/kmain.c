
#include <boot/multiboot.h>
#include <boot/toy_boot.h>
#include <driver/pci/ide.h>
#include <driver/pci/pci.h>
#include <ff.h>

#include "kprintf.h"
#include "mmu/heap.h"
#include "mmu/page.h"
#include "proc/proc.h"
#include "screen/screen.h"
#include "x86/gdt.h"
#include "x86/idt.h"
#include "x86/io.h"
#include "x86/irq.h"
#include "x86/timer.h"

uint8_t temp_stack[1024];

void system_init(BootInfo* info, uint32_t stack) {
  gdt_install(stack);
  page_init(info);
  idt_intall();
  irq_install();
  timer_init();
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

void proc_test() {
  Proc* proc = init_proc();

  proc->regs.cs = USER_CODE_SELECTOR;
  proc->regs.ds = USER_DATA_SELECTOR;
  proc->regs.es = USER_DATA_SELECTOR;
  proc->regs.fs = USER_DATA_SELECTOR;
  proc->regs.ss = USER_DATA_SELECTOR;
  proc->regs.gs = USER_DATA_SELECTOR;
  proc->regs.eip = (uint32_t)task_a;
  proc->regs.esp = (uint32_t)(temp_stack + 1024);
  proc->regs.eflags = 0x1202;

  kprintf("task stack top = %x\n", proc->regs.esp);
  switch_to_ready(proc);
  proc_restart();
}

uint32_t kernel_main(uint32_t eax, uint32_t ebx) {
  if (eax != 0x2BADB002) {
    // eax must be this magic number.
    // other value means boot failed
    return 0x1111;
  }

  multiboot_info_t* mb_info = (multiboot_info_t*)ebx;

  Framebuffer fb_info;
  uint32_t fb_found = 0;

  if (mb_info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
    fb_found = 1;
    fb_info.addr = mb_info->framebuffer_addr;
    fb_info.width = mb_info->framebuffer_width;
    fb_info.height = mb_info->framebuffer_height;
    fb_info.pitch = mb_info->framebuffer_pitch;
    fb_info.bpp = mb_info->framebuffer_bpp / 8;
  }

  if (fb_found == 0) {
    // NO fb found
    return 0x2222;
  }

  screen_init(&fb_info);

  kprintf("kernel begins... \n");

  while (1)
    ;

  return 0;
}

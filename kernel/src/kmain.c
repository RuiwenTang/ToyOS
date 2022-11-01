
#include <boot/toy_boot.h>
#include <driver/pci/pci.h>

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

void kernel_main(BootInfo* boot_info, uint32_t stack) {
  screen_init(&boot_info->frame_buffer);
  screen_set_color(SCREEN_COLOR_WHITE);
  screen_clear();

  kprintf("Now kernel is in charge and never go back to real mode!!\n");

  kprintf("Screen addr is :%x | kernel stack: %x\n",
          boot_info->frame_buffer.addr, stack - 20);

  uint32_t kernel_stack = stack - 4 * 5;

  system_init(boot_info, kernel_stack);

  pci_init();

  // proc test
  // proc_test();

  // enable int manually
  x86_enable_interrupt();

  while (1)
    ;
}


#include <boot/toy_boot.h>

#include "kprintf.h"
#include "mmu/page.h"
#include "screen/screen.h"
#include "x86/gdt.h"
#include "x86/idt.h"

void system_init(BootInfo* info, uint32_t stack) {
  gdt_install(stack);
  idt_intall();
  page_init(info);
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
}

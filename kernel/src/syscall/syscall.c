#include "syscall/syscall.h"

#include <string.h>

#include "kprintf.h"
#include "screen/screen.h"
#include "x86/isr.h"

// this is for debug
static void print_sys_call(StackFrame* frame) {
  // the address need to do memory map, but for now kernel has the proc memory
  // mapping
  char* str = (char*)frame->ebx;
  uint32_t len = strlen(str);
  //   screen_print(str, len, SCREEN_COLOR_GREEN);
  kprintf("sys call print \n");
}

void kernel_sys_call(StackFrame* frame) {
  if (frame->eax == 1) {
    print_sys_call(frame);
  }
}

void sys_call_init() { isr_register_handler(SYS_CALL_INT, kernel_sys_call); }
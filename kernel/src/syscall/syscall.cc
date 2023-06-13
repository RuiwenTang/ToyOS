#include "syscall/syscall.h"

#include <string.h>

#include "fs/vfs.hpp"
#include "kprintf.h"
#include "mmu/page.hpp"
#include "screen/screen.h"
#include "x86/isr.h"

// this is for debug
static void print_sys_call(StackFrame* frame) {
  // the address need to do memory map, but for now kernel has the proc memory
  // mapping
  char* str = (char*)frame->ebx;
  uint32_t len = strlen(str);
  screen_print(str, len, SCREEN_COLOR_GREEN);
}

extern "C" void kernel_sys_call(StackFrame* frame) {
  if (frame->eax == 1) {
    print_sys_call(frame);
  } else if (frame->eax == SYS_CALL_MMAP) {
    mmu::sys_call_mmap(frame);
  } else if (frame->eax == SYS_CALL_UNMAP) {
    mmu::sys_call_unmmap(frame);
  } else if (frame->eax == SYS_CALL_OPEN) {
    fs::sys_call_open(frame);
  } else if (frame->eax == SYS_CALL_CLOSE) {
    fs::sys_call_close(frame);
  }
}

void sys_call_init() { isr_register_handler(SYS_CALL_INT, kernel_sys_call); }
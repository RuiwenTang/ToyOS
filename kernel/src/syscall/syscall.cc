#include "syscall/syscall.h"

#include <string.h>

#include "fs/vfs.hpp"
#include "kprintf.h"
#include "mmu/page.hpp"
#include "proc/proc.h"
#include "screen/screen.h"
#include "x86/isr.h"

// this is for debug
static void print_sys_call(StackFrame* frame) {
  // the address need to do memory map, but for now kernel has the proc memory
  // mapping
  char* str = (char*)frame->ecx;
  uint32_t len = frame->edx;
  screen_print(str, len, SCREEN_COLOR_GREEN);
  frame->eax = len;
}

extern "C" void kernel_sys_call(StackFrame* frame) {
  if (frame->eax == SYS_CALL_EXIT) {
    proc_exit((Proc*)frame);
  } else if (frame->eax == SYS_CALL_WRITE) {
    // File id is stored in ebx
    if (frame->ebx == 1) {
      print_sys_call(frame);
    } else {
      fs::sys_call_write(frame);
    }
  } else if (frame->eax == SYS_CALL_MMAP) {
    mmu::sys_call_mmap(frame);
  } else if (frame->eax == SYS_CALL_UNMAP) {
    mmu::sys_call_unmmap(frame);
  } else if (frame->eax == SYS_CALL_OPEN) {
    fs::sys_call_open(frame);
  } else if (frame->eax == SYS_CALL_CLOSE) {
    fs::sys_call_close(frame);
  } else if (frame->eax == SYS_CALL_READ) {
    fs::sys_call_read(frame);
  } else if (frame->eax == SYS_CALL_SEEK) {
    fs::sys_call_seek(frame);
  } else if (frame->eax == SYS_CALL_GETPID) {
    frame->eax = proc_get_pid(reinterpret_cast<Proc*>(frame));
  }
}

void sys_call_init() { isr_register_handler(SYS_CALL_INT, kernel_sys_call); }

#ifndef TOY_KERNEL_PROC_STACK_FRAME_H
#define TOY_KERNEL_PROC_STACK_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stackframe {
  // pushed by isr handler
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;
  // pushed by pusha
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t kernel_esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  // pushed by isr handler stub
  uint32_t interrupt;
  uint32_t error;
  // pushed by cpu
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;
} StackFrame;

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_PROC_STACK_FRAME_H
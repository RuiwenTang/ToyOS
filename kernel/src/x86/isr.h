#ifndef TOY_X86_ISR_H
#define TOY_X86_ISR_H

#include <stdint.h>

typedef struct {
  uint32_t ds;  // pushed by us
  // pushed by pusha
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t useless;
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
} __attribute__((packed)) Registers;

typedef void (*ISRHandler)(Registers*);

void isr_register_handler(uint32_t interrupt, ISRHandler handler);

#endif  // TOY_X86_ISR_H
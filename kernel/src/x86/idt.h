#ifndef TOY_X86_IDT_H
#define TOY_X86_IDT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // low 16 address bits of handler function
  uint16_t base_low;
  // segment selector
  uint16_t segment_selector;
  // reserved should be zero
  uint8_t reserved;
  // type info
  uint8_t flags;
  // high 16 address bits of handler function
  uint16_t base_high;
} __attribute__((packed)) IDT_ENTRY;

typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) IDT_PTR;

void idt_intall();

// implement in idt.asm
void idt_flush(IDT_PTR* ptr);

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_IDT_H
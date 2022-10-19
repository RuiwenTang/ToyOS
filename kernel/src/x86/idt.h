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

typedef enum {
  IDT_FLAG_GATE_TASK = 0x5,
  IDT_FLAG_GATE_16BIT_INT = 0x6,
  IDT_FLAG_GATE_16BIT_TRAP = 0x7,
  IDT_FLAG_GATE_32BIT_INT = 0xE,
  IDT_FLAG_GATE_32BIT_TRAP = 0xF,

  IDT_FLAG_RING0 = (0 << 5),
  IDT_FLAG_RING1 = (1 << 5),
  IDT_FLAG_RING2 = (2 << 5),
  IDT_FLAG_RING3 = (3 << 5),

  IDT_FLAG_PRESENT = 0x80,
} IDT_FLAGS;

void idt_intall();

void idt_set_gate(uint32_t interrupt, void* base, uint16_t segment,
                  uint8_t flags);

void idt_enable_gate(uint32_t interrupt);

void idt_disable_gate(uint32_t interrupt);

// implement in idt.asm
void idt_flush(IDT_PTR* ptr);

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_IDT_H
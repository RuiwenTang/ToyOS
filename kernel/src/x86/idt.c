#include "x86/idt.h"

#define IDT_COUNT 256

IDT_ENTRY g_idt[IDT_COUNT];

IDT_PTR idt_ptr;

void idt_intall() {
  idt_ptr.base = (uint32_t)&g_idt;
  idt_ptr.limit = sizeof(g_idt) - 1;

  idt_flush(&idt_ptr);
}

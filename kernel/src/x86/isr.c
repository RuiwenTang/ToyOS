#include "x86/isr.h"

#include <stddef.h>

#include "kprintf.h"
#include "x86/idt.h"

ISRHandler g_isr_handler[256];

static const char* const g_Exceptions[] = {
    "Divide by zero error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception ",
    "",
    "",
    "",
    "",
    "",
    "",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "",
};

void c_isr_handler(Registers* registers) {
  if (g_isr_handler[registers->interrupt] != NULL) {
    g_isr_handler[registers->interrupt](registers);
  } else if (registers->interrupt >= 32) {
    kprintf("Unhandled interrupt %d!\n", registers->interrupt);
  } else {
    kprintf("Unhandled exception %d | %s \n", registers->interrupt,
            g_Exceptions[registers->interrupt]);

    kprintf(" eax=%x  ebx=%x  ecx=%x  edx=%x  esi=%x  edi=%x \n",
            registers->eax, registers->ebx, registers->ecx, registers->edx,
            registers->esi, registers->edi);

    kprintf("  esp=%x  ebp=%x  eip=%x  eflags=%x  cs=%x  ds=%x  ss=%x\n",
            registers->esp, registers->ebp, registers->eip, registers->eflags,
            registers->cs, registers->ds, registers->ss);

    kprintf("Kernel PANIC!\n");
    // TODO panic
    while (1)
      ;
  }
}

void isr_register_handler(uint32_t interrupt, ISRHandler handler) {
  g_isr_handler[interrupt] = handler;

  idt_enable_gate(interrupt);
}

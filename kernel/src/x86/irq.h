#ifndef TOY_X86_IRQ_H
#define TOY_X86_IRQ_H

#include "x86/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*IRQHandler)(StackFrame* regs);

void irq_install();

void irq_register_handler(int32_t irq, IRQHandler handler);

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_IRQ_h

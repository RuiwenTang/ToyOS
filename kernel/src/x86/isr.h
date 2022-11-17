#ifndef TOY_X86_ISR_H
#define TOY_X86_ISR_H

#include <stdint.h>

#include "proc/proc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ISRHandler)(StackFrame*);

void isr_register_handler(uint32_t interrupt, ISRHandler handler);

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_ISR_H
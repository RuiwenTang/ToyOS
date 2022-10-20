#ifndef TOY_X86_IO_H
#define TOY_X86_IO_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// implement in io.asm
void x86_outb(uint16_t port, uint8_t value);

uint8_t x86_inb(uint16_t port);

void x86_enable_interrupt();

void x86_disable_interrupt();

#ifdef __cplusplus
}
#endif

#endif // TOY_X86_IO_H

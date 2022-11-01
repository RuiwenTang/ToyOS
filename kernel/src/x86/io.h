#ifndef TOY_X86_IO_H
#define TOY_X86_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// implement in io.asm
void x86_out8(uint16_t port, uint8_t value);
void x86_out16(uint16_t port, uint16_t value);
void x86_out32(uint16_t port, uint32_t value);

uint8_t x86_in8(uint16_t port);
uint16_t x86_in16(uint16_t port);
uint32_t x86_in32(uint16_t port);

void x86_enable_interrupt();

void x86_disable_interrupt();

void x86_iowait();

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_IO_H

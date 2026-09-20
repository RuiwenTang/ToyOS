/*
 * serial.h — console serial output interface
 *
 * The implementation lives in the arch layer (PL011 on QEMU virt, DW 8250
 * on the R5C); the portable kernel code only ever sees these calls — same
 * shape as ToyOS64, so copied modules need no edits.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_SERIAL_H
#define TOYOS_KERNEL_SERIAL_H

#include <toyos/kernel/types.h>

/* Bring the console up. Safe to call more than once. */
void serial_init(void);

/* Write one character. Blocks until TX ready. */
void serial_putchar(char c);

/* Write a null-terminated string. Translates \n to \r\n. */
void serial_puts(const char* s);

/* Write an unsigned 64-bit integer in decimal. */
void serial_print_dec(uint64_t val);

/* Write an unsigned 64-bit integer in hexadecimal (0x prefix). */
void serial_print_hex(uint64_t val);

/* Line-atomic formatted print (SMP): %s %c %u %d %x (0x-prefixed) %p %%.
 * Formats the whole line, then drains under one console-lock hold — one
 * call leaves one intact line even with cores logging concurrently. The
 * per-call locks on puts/dec/hex keep single characters intact but cannot
 * keep a multi-call line from interleaving at call boundaries. */
void serial_printf(const char* fmt, ...);

/* Make console output SMP-safe (cross-core interleave lock). Call exactly
 * once, after the MMU is on (the lock is an atomic on Normal RAM — the
 * pre-MMU window runs unlocked, which is safe: only the BSP exists). */
void serial_smp_arm(void);

#endif /* TOYOS_KERNEL_SERIAL_H */

/*
 * serial_pl011.c — PrimeCell PL011 console (QEMU virt), polled TX
 *
 * Base address is fixed for now: on QEMU's virt machine the PL011 lives
 * at 0x09000000 and the DTB only confirms it. Reading stdout-path out of
 * /chosen is the proper general path once more boards join (R5C uses a
 * DW 8250 instead) — wiring that up is R1 work, not a design change here.
 *
 * Polled TX only at this stage: FR.TXFF until clear, then write DR. No
 * interrupts, no RX — the boot console needs output, nothing else.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

#define PL011_BASE 0x09000000UL

#define PL011_DR (PL011_BASE + 0x00) /* data register */
#define PL011_FR (PL011_BASE + 0x18) /* flag register */
#define PL011_IBRD (PL011_BASE + 0x24)
#define PL011_FBRD (PL011_BASE + 0x28)
#define PL011_LCRH (PL011_BASE + 0x2c)
#define PL011_CR (PL011_BASE + 0x30) /* control register */

#define FR_TXFF (1u << 5) /* TX FIFO full */
#define FR_TXFE (1u << 7) /* TX FIFO empty */
#define CR_UARTEN (1u << 0)
#define CR_TXE (1u << 8)
#define CR_RXE (1u << 9)

static volatile uint32_t* reg(uintptr_t off) { return (volatile uint32_t*)off; }

void serial_init(void) {
  /* QEMU's PL011 model ignores the baud dividers; set the Linux-virt
   * values (115200 8N1) for real-hardware parity. Disable first — the
   * PL011 forbids changing LCRH while enabled. */
  uint32_t cr = *reg(PL011_CR);
  *reg(PL011_CR) = cr & ~CR_UARTEN;
  *reg(PL011_IBRD) = 13;
  *reg(PL011_FBRD) = 1;
  *reg(PL011_LCRH) = (3u << 5) | (1u << 4); /* 8N1, FIFO on */
  *reg(PL011_CR) = CR_UARTEN | CR_TXE | CR_RXE;
}

void serial_putchar(char c) {
  /* Wait for the FIFO to fully drain, not just "not full": polling
   * TXFF races the emulator's async FIFO flush and loses characters
   * (the serial output comes out with periodically swallowed chunks).
   * Draining per character is slow but always correct — real hardware
   * included. Throughput can come back with IRQ-driven TX in R1. */
  while (!(*reg(PL011_FR) & FR_TXFE));
  *reg(PL011_DR) = (uint8_t)c;
}

void serial_puts(const char* s) {
  while (*s) {
    if (*s == '\n') serial_putchar('\r');
    serial_putchar(*s++);
  }
}

void serial_print_dec(uint64_t val) {
  char buf[20];
  int i = 0;

  if (val == 0) {
    serial_putchar('0');
    return;
  }
  while (val > 0) {
    buf[i++] = (char)('0' + (val % 10));
    val /= 10;
  }
  while (--i >= 0) serial_putchar(buf[i]);
}

void serial_print_hex(uint64_t val) {
  static const char digits[] = "0123456789abcdef";
  int i;

  serial_puts("0x");
  if (val == 0) {
    serial_putchar('0');
    return;
  }
  for (i = 60; i >= 0; i -= 4) {
    uint64_t nibble = (val >> i) & 0xf;
    if (nibble == 0 && i > 0 && (val >> i) == 0)
      continue; /* skip leading zeros */
    serial_putchar(digits[nibble]);
  }
}

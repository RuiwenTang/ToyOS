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
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

/* vsnprintf is unavailable freestanding; the format walker below is the
 * whole story (no width/precision — kernel log lines never need it). */
#include <stdarg.h>

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

/* SMP interleave lock. Polled TX reads FR and writes DR per character, so
 * without a lock two cores' lines interleave mid-character. Locked only at
 * putchar (the leaf every path funnels through) — a lock at puts level
 * would self-deadlock against putchar. irqsave flavour: an IRQ handler on
 * the same core must not spin on a lock the interrupted context holds.
 *
 * The lock can only exist once atomics are legal (RAM must be Normal, i.e.
 * after the MMU is on): serial_smp_arm() flips it, called by bootmmu_init.
 * Until then prints are unlocked — safe because only the BSP is running. */
static spinlock_t serial_lock; /* .bss zero = unlocked */
static volatile int serial_smp;

void serial_smp_arm(void) {
  __atomic_store_n(&serial_smp, 1, __ATOMIC_RELEASE);
}

void serial_init(void) {
  /* QEMU's PL011 model ignores the baud divisors; set the Linux-virt
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
  if (__atomic_load_n(&serial_smp, __ATOMIC_ACQUIRE)) {
    spin_lock_irqsave(&serial_lock);
    while (!(*reg(PL011_FR) & FR_TXFE));
    *reg(PL011_DR) = (uint8_t)c;
    spin_unlock_irqrestore(&serial_lock);
    return;
  }
  while (!(*reg(PL011_FR) & FR_TXFE));
  *reg(PL011_DR) = (uint8_t)c;
}

/* Raw emit, NO locking — only for callers already holding serial_lock
 * (serial_printf). Never export this; every other path goes through the
 * locked putchar. */
static void serial_emit(const char* s) {
  while (*s) {
    if (*s == '\n') {
      while (!(*reg(PL011_FR) & FR_TXFE));
      *reg(PL011_DR) = (uint8_t)'\r';
    }
    while (!(*reg(PL011_FR) & FR_TXFE));
    *reg(PL011_DR) = (uint8_t)*s++;
  }
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

/* --- Line-atomic formatted output (SMP) ---
 *
 * serial_puts/print_dec/print_hex each take the console lock per CALL:
 * that keeps characters intact under cross-core contention, but a caller
 * composing a line from several calls can still interleave AT THE CALL
 * BOUNDARIES. serial_printf formats the whole line first and drains it
 * under ONE lock hold — one call, one intact line. Multi-core log sites
 * use it; single-core boot paths may use either.
 *
 * Supported: %s %c %u %d %x (hex, 0x-prefixed per house style) %p %%. */

static void buf_char(char* buf, int* len, char c) {
  if (*len < 118) buf[(*len)++] = c;
}

static void buf_str(char* buf, int* len, const char* s) {
  while (*s) buf_char(buf, len, *s++);
}

static void buf_u64(char* buf, int* len, uint64_t v) {
  char tmp[20];
  int n = 0;

  if (v == 0) {
    buf_char(buf, len, '0');
    return;
  }
  while (v > 0) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  while (--n >= 0) buf_char(buf, len, tmp[n]);
}

static void buf_hex(char* buf, int* len, uint64_t v) {
  static const char digits[] = "0123456789abcdef";
  int started = 0;

  buf_str(buf, len, "0x");
  for (int i = 60; i >= 0; i -= 4) {
    uint64_t nibble = (v >> i) & 0xf;
    if (nibble == 0 && !started && i > 0) continue;
    started = 1;
    buf_char(buf, len, digits[nibble]);
  }
  if (!started) buf_char(buf, len, '0');
}

void serial_printf(const char* fmt, ...) {
  char buf[128];
  int len = 0;
  va_list ap;

  va_start(ap, fmt);
  for (const char* p = fmt; *p; p++) {
    if (*p != '%') {
      buf_char(buf, &len, *p);
      continue;
    }
    p++;
    switch (*p) {
      case 's':
        buf_str(buf, &len, va_arg(ap, const char*));
        break;
      case 'c':
        buf_char(buf, &len, (char)va_arg(ap, int));
        break;
      case 'u':
        buf_u64(buf, &len, va_arg(ap, uint64_t));
        break;
      case 'd': {
        int64_t v = va_arg(ap, int64_t);
        if (v < 0) {
          buf_char(buf, &len, '-');
          v = -v;
        }
        buf_u64(buf, &len, (uint64_t)v);
        break;
      }
      case 'x':
        buf_hex(buf, &len, va_arg(ap, uint64_t));
        break;
      case 'p':
        buf_hex(buf, &len, (uint64_t)(uintptr_t)va_arg(ap, void*));
        break;
      case '%':
        buf_char(buf, &len, '%');
        break;
      default:
        buf_char(buf, &len, '?');
        break;
    }
  }
  va_end(ap);
  buf[len] = '\0';

  /* One lock hold for the whole line: the interleave guarantee. Emits via
   * the raw serial_emit — the locked putchar would self-deadlock here. */
  if (__atomic_load_n(&serial_smp, __ATOMIC_ACQUIRE)) {
    spin_lock_irqsave(&serial_lock);
    serial_emit(buf);
    spin_unlock_irqrestore(&serial_lock);
  } else {
    serial_emit(buf);
  }
}

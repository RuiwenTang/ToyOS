#include "x86/pic.h"

#include <stddef.h>

#include "mmu/heap.h"
#include "x86/io.h"

// TODO support apic

const char module_name[] = "8259 PIC";

#define PIC1_COMMAND_PORT 0x20
#define PIC1_DATA_PORT 0x21
#define PIC2_COMMAND_PORT 0xA0
#define PIC2_DATA_PORT 0xA1

// Initialization Control Word 1
// -----------------------------
//  0   IC4     if set, the PIC expects to receive ICW4 during initialization
//  1   SGNL    if set, only 1 PIC in the system; if unset, the PIC is cascaded
//  with slave PICs
//              and ICW3 must be sent to controller
//  2   ADI     call address interval, set: 4, not set: 8; ignored on x86, set
//  to 0 3   LTIM    if set, operate in level triggered mode; if unset, operate
//  in edge triggered mode 4   INIT    set to 1 to initialize PIC 5-7 ignored on
//  x86, set to 0

enum {
  PIC_ICW1_ICW4 = 0x01,
  PIC_ICW1_SINGLE = 0x02,
  PIC_ICW1_INTERVAL4 = 0x04,
  PIC_ICW1_LEVEL = 0x08,
  PIC_ICW1_INITIALIZE = 0x10
} PIC_ICW1;

// Initialization Control Word 4
// -----------------------------
//  0   uPM     if set, PIC is in 80x86 mode; if cleared, in MCS-80/85 mode
//  1   AEOI    if set, on last interrupt acknowledge pulse, controller
//  automatically performs
//              end of interrupt operation
//  2   M/S     only use if BUF is set; if set, selects buffer master;
//  otherwise, selects buffer slave 3   BUF     if set, controller operates in
//  buffered mode 4   SFNM    specially fully nested mode; used in systems with
//  large number of cascaded controllers 5-7         reserved, set to 0
enum {
  PIC_ICW4_8086 = 0x1,
  PIC_ICW4_AUTO_EOI = 0x2,
  PIC_ICW4_BUFFER_MASTER = 0x4,
  PIC_ICW4_BUFFER_SLAVE = 0x0,
  PIC_ICW4_BUFFERRED = 0x8,
  PIC_ICW4_SFNM = 0x10,
} PIC_ICW4;

enum {
  PIC_CMD_END_OF_INTERRUPT = 0x20,
  PIC_CMD_READ_IRR = 0x0A,
  PIC_CMD_READ_ISR = 0x0B,
} PIC_CMD;

uint32_t g_pic_mask = 0xffff;
bool g_auto_eoi = false;

void i8259_set_mask(uint32_t new_mask) {
  g_pic_mask = new_mask;
  x86_out8(PIC1_DATA_PORT, g_pic_mask & 0xff);
  x86_iowait();
  x86_out8(PIC2_DATA_PORT, g_pic_mask >> 8);
  x86_iowait();
}

uint16_t i8259_get_mask() {
  return x86_in8(PIC1_DATA_PORT) | (x86_in8(PIC2_DATA_PORT) << 8);
}

// implement function

void i8259_configure(uint8_t offset_pic1, uint8_t offset_pic2, bool auto_eoi) {
  i8259_set_mask(0xFFFF);

  // init control word 1
  x86_out8(PIC1_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
  x86_iowait();
  x86_out8(PIC2_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
  x86_iowait();

  // init control word 2
  x86_out8(PIC1_DATA_PORT, offset_pic1);
  x86_iowait();
  x86_out8(PIC2_DATA_PORT, offset_pic2);
  x86_iowait();

  // init control word 3
  x86_out8(PIC1_DATA_PORT, 0x4);  // slave at IRQ2 (0000 0100)
  x86_iowait();
  x86_out8(PIC2_DATA_PORT, 0x2);  // pic2 is cascad identity (0000 0010)
  x86_iowait();

  // init control word 4
  uint8_t icw4 = PIC_ICW1_ICW4;
  if (auto_eoi) {
    icw4 |= PIC_ICW4_AUTO_EOI;
  }

  x86_out8(PIC1_DATA_PORT, icw4);
  x86_iowait();
  x86_out8(PIC2_DATA_PORT, icw4);
  x86_iowait();

  i8259_set_mask(0xFFFF);
}

void i8259_send_eoi(uint32_t irq) {
  if (irq >= 8) {
    x86_out8(PIC2_COMMAND_PORT, PIC_CMD_END_OF_INTERRUPT);
  }

  x86_out8(PIC1_COMMAND_PORT, PIC_CMD_END_OF_INTERRUPT);
}

void i8259_disable() { i8259_set_mask(0xffff); }

void i8259_mask(uint32_t irq) { i8259_set_mask(g_pic_mask | (1 << irq)); }

void i8259_unmask(uint32_t irq) { i8259_set_mask(g_pic_mask & ~(1 << irq)); }

uint16_t i8259_read_irq() {
  x86_out8(PIC1_COMMAND_PORT, PIC_CMD_READ_IRR);
  x86_out8(PIC2_COMMAND_PORT, PIC_CMD_READ_IRR);

  uint16_t v1 = x86_in8(PIC2_COMMAND_PORT);
  uint16_t v2 = x86_in8(PIC2_COMMAND_PORT);

  return (v1 | (v2 << 8));
}

uint16_t i8259_read_isr() {
  x86_out8(PIC1_COMMAND_PORT, PIC_CMD_READ_ISR);
  x86_out8(PIC2_COMMAND_PORT, PIC_CMD_READ_ISR);

  uint16_t v1 = x86_in8(PIC2_COMMAND_PORT);
  uint16_t v2 = x86_in8(PIC2_COMMAND_PORT);

  return (v1 | (v2 << 8));
}

bool i8259_probe() {
  i8259_disable();
  i8259_set_mask(0x1337);

  return i8259_get_mask() == 0x1337;
}

PICDriver* g_pic_driver;

PICDriver* init_pic_driver() {
  g_pic_driver = (PICDriver*)kmalloc(sizeof(PICDriver));

  g_pic_driver->name = module_name;
  g_pic_driver->probe = &i8259_probe;
  g_pic_driver->initialize = &i8259_configure;
  g_pic_driver->disable = &i8259_disable;
  g_pic_driver->send_end_of_interrupt = &i8259_send_eoi;
  g_pic_driver->mask = &i8259_mask;
  g_pic_driver->unmask = &i8259_unmask;

  return g_pic_driver;
}

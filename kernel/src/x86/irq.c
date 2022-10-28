
#include "x86/irq.h"

#include <stddef.h>

#include "kprintf.h"
#include "screen/screen.h"
#include "x86/io.h"
#include "x86/pic.h"

// TODO support apic
#define PIC_REMAP_OFFSET 0x20
#define MODULE "PIC"

IRQHandler g_irq_map[16];

const PICDriver* g_driver = NULL;

void x86_irq_common_stub(StackFrame* regs) {
  int32_t irq = regs->interrupt - PIC_REMAP_OFFSET;

  if (g_irq_map[irq] != NULL) {
    // call handler
    g_irq_map[irq](regs);
  } else {
    // un handled IRQ
    kprintf("Un handled irq at %d\n", irq);
  }

  g_driver->send_end_of_interrupt(irq);
}

void irq_install() {
  const PICDriver* drivers[] = {
      init_pic_driver(),
  };

  for (int32_t i = 0; i < sizeof(drivers); i++) {
    if (drivers[i]->probe()) {
      g_driver = drivers[i];
      break;
    }
  }

  if (g_driver == NULL) {
    screen_set_color(SCREEN_COLOR_RED);
    kprintf("NO PIC found!! \n");
    screen_set_color(SCREEN_COLOR_WHITE);
    return;
  }

  kprintf("Found %s PIC \n", g_driver->name);

  g_driver->initialize(PIC_REMAP_OFFSET, PIC_REMAP_OFFSET + 8, false);

  for (int32_t i = 0; i < 16; i++) {
    isr_register_handler(PIC_REMAP_OFFSET + i, x86_irq_common_stub);
  }

  x86_enable_interrupt();
}

void irq_register_handler(int32_t irq, IRQHandler handler) {
  g_irq_map[irq] = handler;

  g_driver->unmask(irq);
}

#ifndef TOY_X86_PIC_H
#define TOY_X86_PIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char* name;
  bool (*probe)();
  void (*initialize)(uint8_t offset_pic1, uint8_t offset_pic2, bool auto_eoi);
  void (*disable)();
  void (*send_end_of_interrupt)(uint32_t irq);
  void (*mask)(uint32_t irq);
  void (*unmask)(uint32_t irq);
} PICDriver;

PICDriver* init_pic_driver();

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_PIC_H

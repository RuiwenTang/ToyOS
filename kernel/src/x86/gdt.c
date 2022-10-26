#include "x86/gdt.h"

GDT_PTR gdt_ptr;

static GDT_ENTRY gdt[GDT_ENTRIES];

static TSS_ENTRY tss;

TSS_ENTRY *g_tss = &tss;

// implement in gdt.asm
extern void gdt_flush(GDT_PTR *);

extern void tss_flush(uint32_t tss_selector);

void configure_gdt_entry(GDT_ENTRY *entry, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
  entry->base_low = base & 0xFFFF;
  entry->base_middle = (base >> 16) & 0xFF;
  entry->base_high = (base >> 24) & 0xFF;

  entry->limit_low = limit & 0xFFFF;
  entry->granularity = (limit >> 16) & 0x0F;

  entry->granularity |= (gran & 0xF0);
  entry->access = access;
}

void gdt_install(uint32_t kernel_stack) {
  gdt_ptr.limit = (sizeof(GDT_ENTRY) * GDT_ENTRIES) - 1;
  gdt_ptr.base = (size_t)&gdt;

  // null descriptor
  configure_gdt_entry(&gdt[0], 0, 0, 0, 0);

  // kernel code segment
  configure_gdt_entry(
      &gdt[1], 0, 0xFFFFFFFF,
      GDT_FLAG_RING0 | GDT_FLAG_SEGMENT | GDT_FLAG_CODESEG | GDT_FLAG_PRESENT,
      GDT_FLAG_32_BIT | GDT_FLAG_4K_GRAN);

  // kernel data segment
  configure_gdt_entry(
      &gdt[2], 0, 0xFFFFFFFF,
      GDT_FLAG_RING0 | GDT_FLAG_SEGMENT | GDT_FLAG_DATASEG | GDT_FLAG_PRESENT,
      GDT_FLAG_32_BIT | GDT_FLAG_4K_GRAN);

  // user code segment
  configure_gdt_entry(
      &gdt[3], 0, 0xFFFFFFFF,
      GDT_FLAG_RING3 | GDT_FLAG_SEGMENT | GDT_FLAG_CODESEG | GDT_FLAG_PRESENT,
      GDT_FLAG_32_BIT | GDT_FLAG_4K_GRAN);

  // user data segment
  configure_gdt_entry(
      &gdt[4], 0, 0xFFFFFFFF,
      GDT_FLAG_RING3 | GDT_FLAG_SEGMENT | GDT_FLAG_DATASEG | GDT_FLAG_PRESENT,
      GDT_FLAG_32_BIT | GDT_FLAG_4K_GRAN);

  tss.eflags = 0x1202;
  tss.ss0 = 0x10;
  tss.esp0 = kernel_stack;
  tss.cs = 0x0b;
  tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;

  // TODO config tss
  configure_gdt_entry(&gdt[5], (uint32_t)&tss, sizeof(TSS_ENTRY) - 1,
                      GDT_FLAG_PRESENT | GDT_FLAG_TSS | GDT_FLAG_RING0,
                      GDT_FLAG_32_BIT | GDT_FLAG_4K_GRAN);

  gdt_flush(&gdt_ptr);

  tss_flush((5 << 3));
}
#ifndef TOY_X86_GDT_H
#define TOY_X86_GDT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// This segment is a data segment
#define GDT_FLAG_DATASEG 0x02
/// This segment is a code segment
#define GDT_FLAG_CODESEG 0x0a
#define GDT_FLAG_TSS 0x09
#define GDT_FLAG_TSS_BUSY 0x02

#define GDT_FLAG_SEGMENT 0x10
/// Privilege level: Ring 0
#define GDT_FLAG_RING0 0x00
/// Privilege level: Ring 1
#define GDT_FLAG_RING1 0x20
/// Privilege level: Ring 2
#define GDT_FLAG_RING2 0x40
/// Privilege level: Ring 3
#define GDT_FLAG_RING3 0x60
/// Segment is present
#define GDT_FLAG_PRESENT 0x80
/// Segment was accessed
#define GDT_FLAG_ACCESSED 0x01

/**
 * @brief Granularity of segment limit
 * - set: segment limit unit is 4 KB (page size)
 * - not set: unit is bytes
 */
#define GDT_FLAG_4K_GRAN 0x80
/**
 * @brief Default operand size
 * - set: 32 bit
 * - not set: 16 bit
 */
#define GDT_FLAG_16_BIT 0x00
#define GDT_FLAG_32_BIT 0x40
#define GDT_FLAG_64_BIT 0x20

typedef struct {
  /// Lower 16 bits of limit range
  uint16_t limit_low;
  /// Lower 16 bits of base address
  uint16_t base_low;
  /// middle 8 bits of base address
  uint8_t base_middle;
  /// Access bits
  uint8_t access;
  /// Granularity bits
  uint8_t granularity;
  /// Higher 8 bits of base address
  uint8_t base_high;
} __attribute__((packed)) GDT_ENTRY;

typedef struct {
  /// Size of the table in bytes (not the number of entries!)
  uint16_t limit;
  /// Address of the table
  size_t base;
} __attribute__((packed)) GDT_PTR;

// null, kernel code, kernel data, user code, user data, tss
#define GDT_ENTRIES (5 + 1)

/** @brief Installs the global descriptor table
 *
 * The installation involves the following steps:
 * - set up the special GDT pointer
 * - set up the entries in our GDT
 * - finally call gdt_flush() in our assembler file
 *   in order to tell the processor where the new GDT is
 * - update the new segment registers
 */
void gdt_install();

/** @brief Configures and returns a GDT descriptor with chosen attributes
 *
 * Just feed this function with address, limit and the flags
 * you have seen in gdt.h
 *
 */
void configure_gdt_entry(GDT_ENTRY* entry, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran);

#ifdef __cplusplus
}
#endif

#endif  // TOY_X86_GDT_H

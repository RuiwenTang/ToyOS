#ifndef COMMON_ELF_ELF_H
#define COMMON_ELF_ELF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t Elf32_Half;  // Unsigned half int
typedef uint32_t Elf32_Off;   // Unsigned offset
typedef uint32_t Elf32_Addr;  // Unsigned address
typedef uint32_t Elf32_Word;  // Unsigned int
typedef int32_t Elf32_Sword;  // Signed int

#define ELF_NIDENT 16

// elf header struct
typedef struct {
  uint8_t e_ident[ELF_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

enum Elf_Ident {
  EI_MAG0 = 0,        // 0x7F
  EI_MAG1 = 1,        // 'E'
  EI_MAG2 = 2,        // 'L'
  EI_MAG3 = 3,        // 'F'
  EI_CLASS = 4,       // Architecture (32/64)
  EI_DATA = 5,        // Byte Order
  EI_VERSION = 6,     // ELF Version
  EI_OSABI = 7,       // OS Specific
  EI_ABIVERSION = 8,  // OS Specific
  EI_PAD = 9          // Padding
};

enum Elf_Type {
  ET_NONE = 0,    // unknown type
  ET_REL = 1,     // relocatable type
  ET_EXEC = 2,    // executable type
  ET_SHARED = 3,  // shared library type
  ET_CORE = 4,    // core type
};

#define ELFMAG0 0x7F  // e_ident[EI_MAG0]
#define ELFMAG1 'E'   // e_ident[EI_MAG1]
#define ELFMAG2 'L'   // e_ident[EI_MAG2]
#define ELFMAG3 'F'   // e_ident[EI_MAG3]

#define ELFDATA2LSB (1)  // Little Endian
#define ELFCLASS32 (1)   // 32-bit Architecture
#define ELFCLASS64 (2)   // 64-bit Architecture

#define EM_386 (3)        // x86 Machine Type
#define EM_x86_64 (0x3E)  // x86_64 Machine Type
#define EV_CURRENT (1)    // ELF Current Version

// abstract elf file reading and writing
struct Elf32_File;

typedef int (*ef_seek)(struct Elf32_File*, uint32_t offset);
typedef int (*ef_read)(struct Elf32_File*, char* buf, uint32_t size);

typedef struct Elf32_File {
  Elf32_Ehdr header;
  ef_seek impl_seek;
  ef_read impl_read;
} Elf32_File;

// backend implement functions
Elf32_File* elf_open_file(const char* path);

int elf_check_valid(Elf32_File* file);

#ifdef __cplusplus
}
#endif

#endif  // COMMON_ELF_ELF_H
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
  // This member of the structure identifies the object file type
  Elf32_Half e_type;
  // This member specifies the required architecture for an individual file
  Elf32_Half e_machine;
  Elf32_Word e_version;
  /**
   * This member gives the virtual address to which the system
   * first transfers control, thus starting the process.  If
   * the file has no associated entry point, this member holds
   * zero.
   *
   */
  Elf32_Addr e_entry;
  /**
   * This member holds the program header table's file offset
   * in bytes.  If the file has no program header table, this
   * member holds zero.
   *
   */
  Elf32_Off e_phoff;
  /**
   * This member holds the section header table's file offset
   * in bytes.  If the file has no section header table, this
   * member holds zero.
   *
   */
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  // This member holds the ELF header's size in bytes.
  Elf32_Half e_ehsize;
  /**
   * This member holds the size in bytes of one entry in the
   * file's program header table; all entries are the same
   * size.
   *
   */
  Elf32_Half e_phentsize;
  /**
   * This member holds the number of entries in the program
   * header table.  Thus the product of e_phentsize and e_phnum
   * gives the table's size in bytes.  If a file has no program
   * header, e_phnum holds the value zero.
   *
   */
  Elf32_Half e_phnum;
  /**
   * This member holds a sections header's size in bytes.  A
   * section header is one entry in the section header table;
   * all entries are the same size.
   *
   */
  Elf32_Half e_shentsize;
  /**
   * This member holds the number of entries in the section
   * header table.  Thus the product of e_shentsize and e_shnum
   * gives the section header table's size in bytes.  If a file
   * has no section header table, e_shnum holds the value of
   * zero.
   *
   */
  Elf32_Half e_shnum;
  /**
   * This member holds the section header table index of the
   * entry associated with the section name string table.  If
   * the file has no section name string table, this member
   * holds the value SHN_UNDEF.
   *
   */
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

/**
 * @brief
 * An executable or shared object file's program header table is an
 * array of structures, each describing a segment or other
 * information the system needs to prepare the program for
 * execution.  An object file segment contains one or more sections.
 * Program headers are meaningful only for executable and shared
 * object files.  A file specifies its own program header size with
 * the ELF header's e_phentsize and e_phnum members.  The ELF
 * program header is described by the type Elf32_Phdr or Elf64_Phdr
 * depending on the architecture.
 */
typedef struct {
  /**
   * This member of the structure indicates what kind of
   * segment this array element describes or how to interpret
   * the array element's information.
   *
   */
  uint32_t p_type;
  /**
   * This member holds the offset from the beginning of the
   * file at which the first byte of the segment resides.
   *
   */
  Elf32_Off p_offset;
  /**
   * This member holds the virtual address at which the first
   * byte of the segment resides in memory.
   *
   */
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  /**
   * This member holds the number of bytes in the file image of
   * the segment.  It may be zero.
   *
   */
  uint32_t p_filesz;
  /**
   * This member holds the number of bytes in the memory image
   * of the segment.  It may be zero.
   *
   */
  uint32_t p_memsz;
  // This member holds a bit mask of flags relevant to the segment
  uint32_t p_flags;
  /**
   *  This member holds the value to which the segments are
   *  aligned in memory and in the file.  Loadable process
   *  segments must have congruent values for p_vaddr and
   *  p_offset, modulo the page size.  Values of zero and one
   *  mean no alignment is required.  Otherwise, p_align should
   *  be a positive, integral power of two, and p_vaddr should
   *  equal p_offset, modulo p_align.
   *
   */
  uint32_t p_align;
} Elf32_Phdr;

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

/* These constants are for the segment types stored in the image headers */
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7           /* Thread local storage segment */
#define PT_LOOS 0x60000000 /* OS-specific */
#define PT_HIOS 0x6fffffff /* OS-specific */
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff
#define PT_GNU_EH_FRAME (PT_LOOS + 0x474e550)
#define PT_GNU_STACK (PT_LOOS + 0x474e551)
#define PT_GNU_RELRO (PT_LOOS + 0x474e552)
#define PT_GNU_PROPERTY (PT_LOOS + 0x474e553)

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

int elf_enum_phdr(Elf32_File* file, Elf32_Phdr* headers, uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif  // COMMON_ELF_ELF_H
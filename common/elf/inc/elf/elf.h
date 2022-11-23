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

/**
 * A file's section header table lets one locate all the file's
 * sections.  The section header table is an array of Elf32_Shdr or
 * Elf64_Shdr structures.  The ELF header's e_shoff member gives the
 * byte offset from the beginning of the file to the section header
 * table.  e_shnum holds the number of entries the section header
 * table contains.  e_shentsize holds the size in bytes of each
 * entry.
 *
 */
typedef struct {
  // This member specifies the name of the section.  Its value is an index into
  // the section header string table section, giving the location of a
  // null-terminated string.
  uint32_t sh_name;
  // This member categorizes the section's contents and semantics.
  uint32_t sh_type;
  // Sections support one-bit flags that describe miscellaneous attributes.  If
  // a flag bit is set in sh_flags, the attribute is "on" for the section.
  // Otherwise, the attribute is "off" or does not apply.  Undefined attributes
  // are set to zero.
  uint32_t sh_flags;
  // If this section appears in the memory image of a process, this member holds
  // the address at which the section's first byte should reside.  Otherwise,
  // the member contains zero.
  Elf32_Addr sh_addr;
  // This member's value holds the byte offset from the beginning of the file to
  // the first byte in the section. One section type, SHT_NOBITS, occupies no
  // space in the file, and its sh_offset member locates the conceptual
  // placement in the file.
  Elf32_Off sh_offset;
  // This member holds the section's size in bytes.  Unless the section type is
  // SHT_NOBITS, the section occupies sh_size bytes in the file.  A section of
  // type SHT_NOBITS may have a nonzero size, but it occupies no space in the
  // file.
  uint32_t sh_size;
  // This member holds a section header table index link, whose interpretation
  // depends on the section type.
  uint32_t sh_link;
  // This member holds extra information, whose interpretation depends on the
  // section type.
  uint32_t sh_info;
  uint32_t sh_addralign;
  // Some sections hold a table of fixed-sized entries, such as a symbol table.
  // For such a section, this member gives the size in bytes for each entry.
  // This member contains zero if the section does not hold a table of
  // fixed-size entries.
  uint32_t sh_entsize;
} Elf32_Shdr;

/**
 * @brief String and symbol tables
 *
 * String table sections hold null-terminated character sequences,
 * commonly called strings.  The object file uses these strings to
 * represent symbol and section names.  One references a string as
 * an index into the string table section.  The first byte, which is
 * index zero, is defined to hold a null byte ('\0').  Similarly, a
 * string table's last byte is defined to hold a null byte, ensuring
 * null termination for all strings.
 *
 * An object file's symbol table holds information needed to locate
 * and relocate a program's symbolic definitions and references.  A
 * symbol table index is a subscript into this array.
 */
typedef struct {
  /**
   * This member holds an index into the object file's symbol
   * string table, which holds character representations of the
   * symbol names.  If the value is nonzero, it represents a
   * string table index that gives the symbol name.  Otherwise,
   * the symbol has no name.
   *
   */
  uint32_t st_name;
  // This member gives the value of the associated symbol.
  Elf32_Addr st_value;
  // Many symbols have associated sizes.  This member holds zero if the symbol
  // has no size or an unknown size.
  uint32_t st_size;
  // This member specifies the symbol's type and binding attributes:
  unsigned char st_info;
  // This member defines the symbol visibility.
  unsigned char st_other;
  uint16_t st_shndx;
} Elf32_Sym;

/**
 * The .dynamic section contains a series of structures that hold
 * relevant dynamic linking information.  The d_tag member controls
 * the interpretation of d_un.
 *
 */
typedef struct {
  /**
   * This member may have any of the following values:
   *
   * DT_NULL:
   *          Marks end of dynamic section
   * DT_NEEDED:
   *          String table offset to name of a needed library
   * DT_PLTRELSZ:
   *          Size in bytes of PLT relocation entries
   * DT_PLTGOT:
   *          Address of PLT and/or GOT
   * DT_HASH:
   *          Address of symbol hash table
   * DT_STRTAB:
   *          Address of string table
   * DT_SYMTAB:
   *          Address of symbol table
   * DT_RELA:
   *          Address of Rela relocation table
   * DT_RELASZ:
   *          Size in bytes of the Rela relocation table
   * DT_RELAENT:
   *          Size in bytes of a Rela relocation table entry
   * DT_STRSZ:
   *          Size in bytes of string table
   * DT_SYMENT:
   *          Size in bytes of a symbol table entry
   * DT_INIT:
   *          Address of the initialization function
   * DT_FINI:
   *          Address of the termination function
   * DT_SONAME:
   *          String table offset to name of shared object
   * DT_RPATH:
   *          String table offset to library search path(deprecated)
   * DT_SYMBOLIC:
   *          Alert linker to search this shared object before the executable
   *          for symbols
   * DT_REL:
   *          Address of Rel relocation table
   * DT_RELSZ:
   *          Size in bytes of Rel relocation table
   * DT_RELENT:
   *          Size in bytes of a Rel table entry
   * DT_PLTREL:
   *          Type of relocation entry to which the PLT refers (Rela or Rel)
   * DT_DEBUG:
   *          Undefined use for debugging
   * DT_TEXTREL:
   *          Absence of this entry indicates that no relocation entries should
   *          apply to a nonwritable segment
   * DT_JMPREL:
   *          Address of relocation entries associated solely with the PLT
   * DT_BIND_NOW:
   *          Instruct dynamic linker to process all relocations before
   *          transferring control to the executable
   * DT_RUNPATH:
   *          String table offset to library search path
   *
   */
  Elf32_Sword d_tag;
  union {
    // represents integer values with various interpretations
    Elf32_Word d_val;
    // This member represents program virtual addresses.
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

typedef struct elf32_rel {
  /**
   * This member gives the location at which to apply the
   * relocation action.  For a relocatable file, the value is
   * the byte offset from the beginning of the section to the
   * storage unit affected by the relocation.  For an
   * executable file or shared object, the value is the virtual
   * address of the storage unit affected by the relocation.
   *
   */
  Elf32_Addr r_offset;
  Elf32_Word r_info;
} Elf32_Rel;

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

/* sh_type */
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_NUM 12
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

/* sh_flags */
#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_RELA_LIVEPATCH 0x00100000
#define SHF_RO_AFTER_INIT 0x00200000
#define SHF_MASKPROC 0xf0000000

/* This info is needed when parsing the symbol table */
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_COMMON 5
#define STT_TLS 6

/* This is the info that is needed to parse the dynamic section of the file */
#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_SYMBOLIC 16
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_ENCODING 32
#define OLD_DT_LOOS 0x60000000
#define DT_LOOS 0x6000000d
#define DT_HIOS 0x6ffff000
#define DT_VALRNGLO 0x6ffffd00
#define DT_VALRNGHI 0x6ffffdff
#define DT_ADDRRNGLO 0x6ffffe00
#define DT_ADDRRNGHI 0x6ffffeff
#define DT_VERSYM 0x6ffffff0
#define DT_RELACOUNT 0x6ffffff9
#define DT_RELCOUNT 0x6ffffffa
#define DT_FLAGS_1 0x6ffffffb
#define DT_VERDEF 0x6ffffffc
#define DT_VERDEFNUM 0x6ffffffd
#define DT_VERNEED 0x6ffffffe
#define DT_VERNEEDNUM 0x6fffffff
#define OLD_DT_HIOS 0x6fffffff
#define DT_LOPROC 0x70000000
#define DT_HIPROC 0x7fffffff

#define ELF32_R_SYM(x) ((x) >> 8u)
#define ELF32_R_TYPE(x) ((x)&0xffu)

#define R_386_NONE 0
#define R_386_32 1
#define R_386_PC32 2
#define R_386_GOT32 3
#define R_386_PLT32 4
#define R_386_COPY 5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8
#define R_386_TLS_TPOFF 14

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

int elf_close_file(Elf32_File* file);

int elf_check_valid(Elf32_File* file);

int elf_enum_phdr(Elf32_File* file, Elf32_Phdr* headers, uint32_t* count);

int elf_enum_shdr(Elf32_File* file, Elf32_Shdr* sections, uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif  // COMMON_ELF_ELF_H
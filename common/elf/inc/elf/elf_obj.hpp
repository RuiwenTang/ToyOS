#ifndef COMMON_ELF_INC_ELF_OBJ_HPP
#define COMMON_ELF_INC_ELF_OBJ_HPP

#include <elf/elf.h>

class ElfObject {
 public:
  ElfObject(const char* path);
  virtual ~ElfObject() = default;

  bool Load(bool is_root);

  uint32_t GetPhdrCount() const { return m_phdr_count; }

  uint32_t GetMemoryBase() const { return m_mem_base; }

  uint32_t GetMemorySize() const { return m_mem_size; }

 protected:
  virtual bool OnResizePhdrs(uint32_t count) = 0;

  virtual Elf32_Phdr* OnGetPhdrs() = 0;

  virtual bool OnResizeShdrs(uint32_t count) = 0;

  virtual bool OnAllocateElfDyn(uint32_t count) = 0;

  virtual Elf32_Shdr* OnGetShdrs() = 0;

  virtual Elf32_Dyn* OnGetDyns() = 0;

  virtual bool OnAllocateMemory(uint32_t* base, uint32_t* size) = 0;

  virtual void* OnVirtualToPhy(uint32_t v_addr) = 0;

 private:
  bool CheckHeader();

  bool LoadPhdrs();

  void CalculateMemSize();

  bool LoadIntoMemory();

  bool ReadDynamicTable();

  void ReadCopyRelocations();

  void LoadSymbols();

  bool Relocation();

 private:
  ElfObject* m_root = 0;
  const char* m_lib_path;
  Elf32_File* m_elf_file;
  int32_t m_mem_size = 0;
  uint32_t m_mem_base = 0;
  uint32_t m_phdr_count = 0;
  uint32_t m_dyn_count = 0;
  uint32_t m_shdr_count = 0;
  uint32_t m_allocaed_base = 0;
  uint32_t m_allocated_size = 0;
  uint32_t m_current_brk = 0;
};

#endif  // COMMON_ELF_INC_ELF_OBJ_HPP
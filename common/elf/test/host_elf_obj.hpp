#include <elf/elf_obj.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct VirtualMemoryRegion {
  uint32_t base;
  uint32_t size;

  std::vector<uint8_t> data;
};

class HostElfObject : public ElfObject {
 public:
  HostElfObject(const char* path) : ElfObject(path) {}
  ~HostElfObject() override;

  void AddSubLibrary(HostElfObject* lib);

 protected:
  bool OnResizePhdrs(uint32_t count) override;

  Elf32_Phdr* OnGetPhdrs() override;

  bool OnResizeShdrs(uint32_t count) override;

  Elf32_Shdr* OnGetShdrs() override;

  bool OnAllocateElfDyn(uint32_t count) override;

  Elf32_Dyn* OnGetDyns() override;

  bool OnAllocateLibNames(uint32_t count) override;

  char** OnGetLibNames() override;

  bool OnAllocateMemory(uint32_t* base, uint32_t* size) override;

  void* OnVirtualToPhy(uint32_t v_addr) override;

  void OnAddGlobalSymbol(char* name, uint32_t addr) override;

  void OnAddSymbol(char* name, uint32_t addr) override;

  uint32_t OnFindSymbol(char* name) override;

  uint32_t OnFindGlobalSymbol(char* name) override;

  uint32_t OnGetTotalLibCount() override;

  ElfObject** OnGetTotalLibs() override;

 private:
  std::vector<Elf32_Phdr> m_phdrs = {};
  std::vector<Elf32_Shdr> m_shdrs = {};
  std::vector<Elf32_Dyn> m_dyns = {};
  std::vector<char*> m_lib_names = {};
  std::vector<std::unique_ptr<VirtualMemoryRegion>> m_virtual_memory = {};
  std::map<std::string, uint32_t> m_global_symbols = {};
  std::map<std::string, uint32_t> m_symbols = {};
  std::vector<HostElfObject*> m_sub_libs = {};
};

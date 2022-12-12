
#include "host_elf_obj.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

std::filesystem::path g_root_path;
std::vector<std::filesystem::path> g_loaded_path;

ElfObject *ElfObject::OpenLib(ElfObject *root, char *path) {
  if (root == nullptr) {
    std::filesystem::path temp_path{path};

    if (temp_path.is_absolute()) {
      temp_path.remove_filename();
      g_root_path = temp_path;
    }

    return new HostElfObject(path);
  }

  auto lib_path = g_root_path;
  lib_path.append(path);

  g_loaded_path.emplace_back(lib_path);

  auto lib = new HostElfObject(g_loaded_path.back().c_str());

  static_cast<HostElfObject *>(root)->AddSubLibrary(lib);

  return lib;
}

HostElfObject::~HostElfObject() {
  for (auto lib : m_sub_libs) {
    delete lib;
  }
}

void HostElfObject::AddSubLibrary(HostElfObject *lib) {
  m_sub_libs.emplace_back(lib);
  lib->SetRoot(this);
}

bool HostElfObject::OnResizePhdrs(uint32_t count) {
  m_phdrs.resize(count);
  return true;
}

Elf32_Phdr *HostElfObject::OnGetPhdrs() { return m_phdrs.data(); }

bool HostElfObject::OnResizeShdrs(uint32_t count) {
  m_shdrs.resize(count);
  return true;
}

Elf32_Shdr *HostElfObject::OnGetShdrs() { return m_shdrs.data(); }

bool HostElfObject::OnAllocateElfDyn(uint32_t count) {
  m_dyns.resize(count);
  return true;
}

Elf32_Dyn *HostElfObject::OnGetDyns() { return m_dyns.data(); }

bool HostElfObject::OnAllocateLibNames(uint32_t count) {
  m_lib_names.resize(count);
  return true;
}

char **HostElfObject::OnGetLibNames() { return m_lib_names.data(); }

bool HostElfObject::OnAllocateMemory(uint32_t *base, uint32_t *size) {
  auto it = std::find_if(
      m_virtual_memory.begin(), m_virtual_memory.end(),
      [this](std::unique_ptr<VirtualMemoryRegion> const &region) {
        uint32_t begin = GetMemoryBase();
        uint32_t end = begin + GetMemorySize();

        if (region->base <= begin && begin <= region->base + region->size) {
          return true;
        }

        if (region->base <= end && end <= region->base + region->size) {
          return true;
        }

        return false;
      });
  if (it != m_virtual_memory.end()) {
    return false;
  }

  std::unique_ptr<VirtualMemoryRegion> region{new VirtualMemoryRegion};

  region->base = GetMemoryBase();
  region->size = GetMemorySize();
  region->data.resize(region->size);

  if (base) {
    *base = region->base;
  }

  if (size) {
    *size = region->size;
  }

  m_virtual_memory.emplace_back(std::move(region));

  return true;
}

void *HostElfObject::OnVirtualToPhy(uint32_t v_addr) {
  auto it = std::find_if(
      m_virtual_memory.begin(), m_virtual_memory.end(),
      [v_addr](std::unique_ptr<VirtualMemoryRegion> const &region) {
        if (region->base <= v_addr && v_addr <= region->base + region->size) {
          return true;
        }

        return false;
      });

  if (it == m_virtual_memory.end()) {
    return (void *)static_cast<uint64_t>(v_addr);
  }

  uint32_t offset = v_addr - (*it)->base;
  return (*it)->data.data() + offset;
}

void HostElfObject::OnAddGlobalSymbol(char *name, uint32_t addr) {
  name = (char *)OnVirtualToPhy((uint64_t)name);

  auto it = m_global_symbols.find(name);
  if (it != m_global_symbols.end()) {
    return;
  }

  m_global_symbols.insert(std::make_pair(std::string(name), addr));
}

void HostElfObject::OnAddSymbol(char *name, uint32_t addr) {
  name = (char *)OnVirtualToPhy((uint64_t)name);

  auto it = m_symbols.find(name);
  if (it != m_symbols.end()) {
    return;
  }

  m_symbols.insert(std::make_pair(std::string(name), addr));
}

uint32_t HostElfObject::OnFindSymbol(char *name) {
  name = (char *)OnVirtualToPhy((uint32_t) reinterpret_cast<uint64_t>(name));

  auto it = m_symbols.find(name);
  if (it == m_symbols.end()) {
    return 0;
  }

  return it->second;
}

uint32_t HostElfObject::OnFindGlobalSymbol(char *name) {
  name = (char *)OnVirtualToPhy((uint32_t) reinterpret_cast<uint64_t>(name));

  auto it = m_global_symbols.find(name);
  if (it == m_global_symbols.end()) {
    return 0;
  }

  return it->second;
}

uint32_t HostElfObject::OnGetTotalLibCount() { return m_sub_libs.size(); }

ElfObject **HostElfObject::OnGetTotalLibs() {
  return reinterpret_cast<ElfObject **>(m_sub_libs.data());
}

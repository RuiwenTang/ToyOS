#ifndef TOY_COMMON_ELF_TEST_ELF_ELF_FILE_HOST_HPP
#define TOY_COMMON_ELF_TEST_ELF_ELF_FILE_HOST_HPP

#include <elf/elf_file.hpp>
#include <fstream>

namespace test {

class ElfFileHost : public ElfFile {
 public:
  ElfFileHost();
  ~ElfFileHost() override;

 protected:
  bool OnOpen(const char* file_name) override;
  void OnClose() override;
  bool OnSeek(uint32_t offset) override;
  bool OnRead(char* buf, uint32_t size) override;

 private:
  std::fstream m_file;
};

}  // namespace test

#endif  // TOY_COMMON_ELF_TEST_ELF_ELF_FILE_HOST_HPP
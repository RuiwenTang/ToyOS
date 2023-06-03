#ifndef TOY_COMMON_ELF_INC_ELF_ELF_FILE_HPP
#define TOY_COMMON_ELF_INC_ELF_ELF_FILE_HPP

#include <elf/elf.h>

class ElfFile {
 public:
  ElfFile();
  virtual ~ElfFile();

  /**
   * Opens an ELF file.
   *
   * @param file_name the name of the file to be opened
   *
   * @return true if the file is successfully opened and valid, false otherwise
   *
   * @throws None
   */
  bool Open(const char* file_name);

  void Close();

  bool IsValid() const { return m_is_valid; }

  /**
   * Enumerates program headers of the ELF file format.
   *
   * @param header a pointer to the ELF header structure
   * @param count a pointer to the number of program headers
   *
   * @return a boolean indicating if the function executed successfully
   *
   * @throws None
   */
  bool EnumPhdr(Elf32_Phdr* header, uint32_t* count);

  uint32_t GetEntryPoint() const;

  bool Seek(uint32_t offset);

  bool Read(char* buf, uint32_t size);

 protected:
  virtual bool OnOpen(const char* file_name) = 0;

  virtual void OnClose() = 0;

  virtual bool OnSeek(uint32_t offset) = 0;

  virtual bool OnRead(char* buf, uint32_t size) = 0;

 private:
  bool CheckValid();

 private:
  Elf32_Ehdr m_elf_header;
  bool m_is_valid = false;
};

#endif  // TOY_COMMON_ELF_INC_ELF_ELF_FILE_HPP
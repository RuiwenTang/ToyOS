
#pragma once

#include <ext4.h>

#include <fstream>
#include <string>

class Ext4File {
 public:
  Ext4File(std::string path);
  ~Ext4File();

  bool IsValid() const;

  bool Format(const std::string& type);

  bool Install(const std::string& src, const std::string& dst);

  void Seekg(uint64_t offset);

  void Read(char* buf, uint64_t size);

  void Seekp(uint64_t offset);

  void Write(char* buf, uint64_t size);

 private:
  bool Init();

  bool LoadPartionDev();

 private:
  std::string m_file_path;
  std::fstream m_file_stream = {};

  ext4_blockdev m_sub_dev = {};
};

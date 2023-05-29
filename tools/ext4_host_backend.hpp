
#pragma once

#include <ext4.h>

#include <fstream>
#include <string>

class Ext4File {
 public:
  Ext4File(std::string path);
  ~Ext4File();

  bool Format(const std::string& type);

  bool Install(const std::string& src, const std::string& dst);

 private:
  bool Init();

 private:
  std::string m_file_path;
  std::fstream m_file_stream = {};
};
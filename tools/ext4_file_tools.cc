#include <ext4.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>

#include "ext4_host_backend.hpp"

cxxopts::Options define_opts() {
  cxxopts::Options options("ext4-tools", "tools to edit ext4 img file");

  // target file path
  {
    cxxopts::Option file_opt("f,file", "target img file",
                             cxxopts::value<std::string>());
    options.add_option("input", file_opt);
  }

  // target filesystem type
  {
    cxxopts::Option fmt_opt("t,type", "target file system type",
                            cxxopts::value<std::string>());
    options.add_option("fmt", fmt_opt);
  }

  // copy file
  {
    cxxopts::Option src_file("s,src", "src file path",
                             cxxopts::value<std::string>());
    cxxopts::Option dst_file("d,dst", "dst file path",
                             cxxopts::value<std::string>());

    options.add_option("install", src_file);
    options.add_option("install", dst_file);
  }

  auto a = options.add_options();
  a("h,help", "Print usage");

  return options;
}

int format_img(const cxxopts::ParseResult& res) {
  std::string img_path{};

  std::string file_type{};

  if (res.count("file")) {
    img_path = res["file"].as<std::string>();
  }

  if (res.count("type")) {
    file_type = res["type"].as<std::string>();
  }

  if (img_path.empty() || file_type.empty()) {
    return -1;
  }

  Ext4File ext4_file(img_path);

  if (ext4_file.Format(file_type)) {
    return 0;
  } else {
    return -1;
  }
}

int main(int argc, const char** argv) {
  auto opts = define_opts();

  auto res = opts.parse(argc, argv);

  if (res.count("help")) {
    std::cout << opts.help() << std::endl;
    return 0;
  }

  if (res.count("type")) {
    return format_img(res);
  }

  return 0;
}
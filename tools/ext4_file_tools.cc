#include <ext4.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>

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

int main(int argc, const char** argv) {
  auto opts = define_opts();

  auto res = opts.parse(argc, argv);

  if (res.count("help")) {
    std::cout << opts.help() << std::endl;
    return 0;
  }

  if (res.count("file")) {
    std::cout << "target file: " << res["file"].as<std::string>() << std::endl;
    return 0;
  }

  return 0;
}
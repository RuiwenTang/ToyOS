#include <ext4.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>

cxxopts::Options define_opts() {
  cxxopts::Options options("ext4-tools", "tools to edit ext4 img file");

  auto a = options.add_options();
  a("f,file", "target img file", cxxopts::value<std::string>());
  a("h,help", "Print usage");

  auto f = options.add_options("fmt");
  f("t,type", "file system type", cxxopts::value<std::string>());

  return options;
}

int main(int argc, const char** argv) {
  auto opts = define_opts();

  auto res = opts.parse(argc, argv);

  if (res.count("help")) {
    std::cout << opts.help() << std::endl;
    return 0;
  }

  return 0;
}
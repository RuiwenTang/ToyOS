
cxxopts_dep = subproject('cxxopts').get_variable('cxxopts_dep')

ext4_file_tool = executable(
  'ext4_file_tool',
  native: true,
  cpp_args: [
    '-std=c++17',
  ],
  include_directories: ['.'],
  sources: [
    'ext4_file_tools.cc',
    'ext4_host_backend.cc',
    'ext4_host_backend.hpp',
  ],
  dependencies: [
    cxxopts_dep,
    ext4fs_dep,
  ],
)



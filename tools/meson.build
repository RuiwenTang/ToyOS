
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
    ext2fs_dep,
  ],
)

mk_disk_img = custom_target(
  'mk_disk_img',
  output: 'ext4_disk.img',
  input: [  ],
  command: [
    ext4_file_tool, '-f', '@OUTPUT@', '-t', 'ext4'
  ]
)

install_boot = custom_target(
  'install_boot',
  output: 'install_limine_boot.ini',
  command: [
    limine_deploy, mk_disk_img
  ]
)

install_limine_sys = custom_target(
  'install_limine_sys',
  output: 'install_limine_sys.ini',
  command: [
    ext4_file_tool, '-f', mk_disk_img, '-s', limine_sys, '-d', '/boot/limine.sys'
  ]
)

install_limine_cfg = custom_target(
  'install_limine_cfg',
  output: 'install_limine_cfg.ini',
  command: [
    ext4_file_tool, '-f', mk_disk_img, '-s', limine_cfg, '-d', '/boot/limine.cfg'
  ]
)

install_kernel = custom_target(
  'install_kernel',
  output: 'install_kernel.ini',
  command: [
    ext4_file_tool, '-f', mk_disk_img, '-s', kernel_sys, '-d', '/boot/kernel.sys'
  ]
)



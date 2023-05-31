
prog_python = import('python').find_installation('python3')

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

mk_floppy = executable(
  'mk_floppy',
  native: true,
  c_args: ['-DFF_USE_MKFS=1'],
  sources: [
    'mk_floppy.cc',
  ],
  cpp_args: [
    '-std=c++17',
  ],
  dependencies: [ fatfs_dep ],
)

copy_apps = executable(
  'copy_apps',
  native: true,
  c_args: ['-DFF_USE_MKFS=1'],
  sources: [
    'copy_apps.cc',
  ],
  cpp_args: [
    '-std=c++17',
  ],
  dependencies: [ fatfs_dep ],
)

disk_img = custom_target(
  'gen_disk',
  output: 'disk.img',
  input: ['gen_floppy.py', mk_floppy, limine_sys, limine_cfg, kernel_sys],
  command: [
    prog_python, '@INPUT@', '@OUTPUT@',
  ],
)

full_img = custom_target(
  'install_app',
  output: 'full_img',
  input: ['install_apps.py', copy_apps, disk_img, init_bin],
  command: [
    prog_python, '@INPUT@', '@OUTPUT@',
  ]
)

install_limine = custom_target(
  'install_limine',
  output: 'boot_disk.img',
  input: ['install_limine.py',limine_deploy, disk_img, full_img],
  command: [
    prog_python,'@INPUT@', '@OUTPUT@',
  ]
)

mk_disk_img = custom_target(
  'mk_disk_img',
  output: 'ext4_disk.img',
  input: [  ],
  command: [
    ext4_file_tool, '-f', '@OUTPUT@', '-t', 'ext4'
  ]
)

install_boot = run_target(
  'install_boot',
  command: [
    limine_deploy, mk_disk_img
  ]
)

install_limine_sys = run_target(
  'install_limine_sys',
  command: [
    ext4_file_tool, '-f', mk_disk_img, '-s', limine_sys, '-d', '/boot/limine.sys'
  ]
)



prog_python = import('python').find_installation('python3')

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

disk_img = custom_target(
  'gen_disk',
  output: 'disk.img',
  input: ['gen_floppy.py', mk_floppy, limine_sys, limine_cfg, kernel_sys],
  command: [
    prog_python, '@INPUT@', '@OUTPUT@',
  ],
)

install_limine = custom_target(
  'install_limine',
  output: 'boot_disk.img',
  input: ['install_limine.py',limine_deploy, disk_img],
  command: [
    prog_python,'@INPUT@', '@OUTPUT@',
  ]
)


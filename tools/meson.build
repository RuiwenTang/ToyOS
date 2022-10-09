
prog_python = import('python').find_installation('python3')

mk_floppy = executable(
  'mk_floppy',
  native: true,
  sources: [
    'fatfs/diskio.c',
    'fatfs/diskio.h',
    'fatfs/ff.c',
    'fatfs/ff.h',
    'fatfs/ffconf.h',
    'fatfs/ffsystem.c',
    'mk_floppy.cc',
  ],
  include_directories: [
    'fatfs',
  ],
  cpp_args: [
    '-std=c++17',
  ]
)

disk_img = custom_target(
  'gen_disk',
  output: 'disk.img',
  input: ['gen_floppy.py', mk_floppy, stage1_boot, stage2_bin],
  command: [
    prog_python, '@INPUT@', '@OUTPUT@',
  ],
)


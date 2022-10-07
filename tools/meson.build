

executable(
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




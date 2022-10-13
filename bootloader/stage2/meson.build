

stage2_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
  '-lgcc',
]

stage2_c_args = [
  '-ffreestanding',
  '-fno-builtin',
  '-nostdlib',
]

# external dependency
buildin_font_inc = subproject('font8x8').get_variable('font8x8_incdir')


stage2_bin = executable(
    'stage2.bin',
    sources: [
        nasm_c.process(['stage2.asm', 'x86/bios_call.asm']),
        'disk/fat.c',
        'disk/fat.h',
        'disk/mbr.c',
        'disk/mbr.h',
        'printf.c',
        'printf.h',
        'screen/screen.c',
        'screen/screen.h',
        'stage2.c',
        'x86/bios.h',
        'x86/vbe.h',
    ],
    include_directories: [
      '.',
      buildin_font_inc
    ],
    link_args: stage2_link_args,
    c_args: stage2_c_args,
    cpp_args: [],
)

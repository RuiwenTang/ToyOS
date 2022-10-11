
stage2_asm = generator(nasm,
  output : '@BASENAME@.o',
  arguments : [
    '-f', 'elf32',
    '-i', meson.current_source_dir() + '/',
    '-i', join_paths(meson.current_source_dir(), ''),
    '@INPUT@',
    '-o', '@OUTPUT@'])


stage2_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
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
        stage2_asm.process(['stage2.asm', 'x86/bios_call.asm']),
        'bios_vbe.h',
        'screen/screen.c',
        'screen/screen.h',
        'stage2.c',
        'x86/bios.h',
    ],
    include_directories: [
      '.',
      buildin_font_inc
    ],
    link_args: stage2_link_args,
    c_args: stage2_c_args,
    cpp_args: [],
)

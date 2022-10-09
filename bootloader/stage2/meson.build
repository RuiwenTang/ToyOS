
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
  '-nostdinc',
]

stage2_bin = executable(
    'stage2.bin',
    sources: [
        stage2_asm.process(['stage2.asm']),
        'stage2.c',
    ],
    include_directories: [],
    link_args: stage2_link_args,
    c_args: stage2_c_args,
    cpp_args: [],
)

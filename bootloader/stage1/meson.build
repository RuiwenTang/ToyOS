
boot_c_args = [
  '-ffreestanding',
  '-fno-builtin',
  '-nostdlib',
  '-nostdinc',
]

nasm = find_program('nasm', required: true)

asm_gen = generator(nasm,
  output : '@BASENAME@.o',
  arguments : [
    '-f', 'elf32',
    '-i', meson.current_source_dir() + '/',
    '-i', join_paths(meson.current_source_dir(), ''),
    '@INPUT@',
    '-o', '@OUTPUT@'])

boot_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
]



executable(
  'boot.bin',
  asm_gen.process('boot.asm'),
  include_directories: [],
  link_args: boot_link_args,
  c_args: boot_c_args,
  cpp_args: [],
)





stage1_asm = generator(nasm,
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



stage1_boot = executable(
  'boot.bin',
  stage1_asm.process(['boot.asm']),
  include_directories: [],
  link_args: boot_link_args,
  c_args: [],
  cpp_args: [],
)


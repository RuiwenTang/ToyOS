

boot_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
]



stage1_boot = executable(
  'boot.bin',
  nasm_c.process(['boot.asm']),
  include_directories: [],
  link_args: boot_link_args,
  c_args: [],
  cpp_args: [],
)


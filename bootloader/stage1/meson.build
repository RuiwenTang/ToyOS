
boot_c_args = [
  '-ffreestanding',
  '-fno-builtin',
  '-nostdlib',
  '-nostdinc',
]

boot_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
]



executable(
  'boot.bin',
  sources: [
    'boot.S',
  ],
  include_directories: [],
  link_args: boot_link_args,
  c_args: boot_c_args,
  cpp_args: [],
)


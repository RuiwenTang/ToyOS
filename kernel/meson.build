

kernel_src = [
  nasm_c.process(['src/boot/entry.asm']),
  'src/kmain.c',
]

kernel_c_flags = [
  '-ffreestanding',
  '-fno-builtin',
  '-nostdlib',
  '-nostdinc++',
]

kernel_cpp_args = [
    '-fno-exceptions',
    '-std=c++14',
]

kernel_link_args = [
  '-nostdlib',
  '-T', meson.current_source_dir() + '/linker.ld',
  '-lgcc',
]

kernel_sys = executable(
    'kernel.sys',
    sources: kernel_src,
    include_directories: [
      '.',
    ],
    link_args: kernel_link_args,
    c_args: kernel_c_flags,
    cpp_args: kernel_cpp_args,
)


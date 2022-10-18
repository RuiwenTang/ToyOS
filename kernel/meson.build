
# external dependency
buildin_font_inc = subproject('font8x8').get_variable('font8x8_incdir')

kernel_src = [
  nasm_c.process(['src/boot/entry.asm', 'src/x86/gdt.asm']),
  'src/kmain.c',
  'src/kprintf.c',
  'src/kprintf.h',
  'src/screen/screen.c',
  'src/screen/screen.h',
  'src/x86/gdt.c',
  'src/x86/gdt.h',
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
      './src',
      buildin_font_inc,
    ],
    link_args: kernel_link_args,
    c_args: kernel_c_flags,
    cpp_args: kernel_cpp_args,
    dependencies: [toy_boot],
)


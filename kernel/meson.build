
# external dependency
buildin_font_inc = subproject('font8x8').get_variable('font8x8_incdir')
umm_malloc = subproject('umm_malloc').get_variable('umm_malloc_dep')

kernel_src = [
  nasm_c.process([
    'src/boot/entry.asm',
    'src/x86/gdt.asm',
    'src/x86/idt.asm',
    'src/x86/isr.asm',
    'src/x86/io.asm',
    'src/mmu/page.asm',
  ]),
  'klibc/stdio.h',
  'klibc/string.c',
  'klibc/string.h',
  'src/cxx_runtime.cc',
  'src/kmain.cc',
  'src/kprintf.c',
  'src/kprintf.h',
  'src/mmu/heap.c',
  'src/mmu/heap.h',
  'src/mmu/page.cc',
  'src/mmu/page.h',
  'src/mmu/page_allocator.cc',
  'src/mmu/page_allocator.hpp',
  'src/screen/screen.c',
  'src/screen/screen.h',
  'src/x86/gdt.c',
  'src/x86/gdt.h',
  'src/x86/idt.c',
  'src/x86/idt.h',
  'src/x86/io.h',
  'src/x86/isr.c',
  'src/x86/isr.h',
  'src/x86/pic.c',
  'src/x86/pic.h',
]

kernel_c_flags = [
  '-ffreestanding',
  '-fno-builtin',
  '-nostdlib',
]

kernel_cpp_args = [
    '-fno-exceptions',
    '-ffreestanding',
    '-fno-builtin',
    '-std=c++14',
    '-nostdinc++',
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
      'klibc',
      buildin_font_inc,
    ],
    link_args: kernel_link_args,
    c_args: kernel_c_flags,
    cpp_args: kernel_cpp_args,
    dependencies: [toy_boot, umm_malloc],
)




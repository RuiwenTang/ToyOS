
# external dependency
buildin_font_inc = subproject('font8x8').get_variable('font8x8_incdir')
umm_malloc = subproject('umm_malloc').get_variable('umm_malloc_dep')

kernel_src = [
  nasm_c.process([
    'src/boot/entry.asm',
    'src/mmu/page.asm',
    'src/proc/proc.asm',
    'src/x86/gdt.asm',
    'src/x86/idt.asm',
    'src/x86/io.asm',
    'src/x86/isr.asm',
  ]),
  'include/driver/pci/ide.h',
  'include/driver/pci/pci.h',
  'klibc/stdio.h',
  'klibc/string.c',
  'klibc/string.h',
  'src/driver/pci/ide.c',
  'src/driver/pci/pci.c',
  'src/elf/elf_impl.c',
  'src/elf/elf_loader.c',
  'src/elf/elf_loader.h',
  'src/fs/fatfs_backend.c',
  'src/kmain.c',
  'src/kprintf.c',
  'src/kprintf.h',
  'src/mmu/heap.c',
  'src/mmu/heap.h',
  'src/mmu/page.c',
  'src/mmu/page.h',
  'src/mmu/palloc.c',
  'src/mmu/palloc.h',
  'src/proc/proc.c',
  'src/proc/proc.h',
  'src/screen/screen.c',
  'src/screen/screen.h',
  'src/x86/gdt.c',
  'src/x86/gdt.h',
  'src/x86/idt.c',
  'src/x86/idt.h',
  'src/x86/io.h',
  'src/x86/irq.c',
  'src/x86/irq.h',
  'src/x86/isr.c',
  'src/x86/isr.h',
  'src/x86/pic.c',
  'src/x86/pic.h',
  'src/x86/timer.c',
  'src/x86/timer.h',
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
      'include',
      buildin_font_inc,
    ],
    link_args: kernel_link_args,
    c_args: kernel_c_flags,
    cpp_args: kernel_cpp_args,
    dependencies: [toy_boot, umm_malloc, fatfs_dep, elf_dep],
)

gtest = subproject('gtest').get_variable('gtest_dep')

executable(
  'mmu_alloc_test',
  native: true,
  sources: [ 'src/mmu/palloc.h', 'src/mmu/palloc.c', 'src/mmu/palloc_test.cc' ],
  include_directories: [
    './src',
  ],
  c_args: [ '-DTOY_DEBUG' ],
  cpp_args: [ '-std=c++17', '-DTOY_DEBUG' ],
  dependencies: [ gtest ],
  )


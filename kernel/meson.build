
# external dependency
buildin_font_inc = subproject('font8x8').get_variable('font8x8_incdir')
umm_malloc = subproject('umm_malloc').get_variable('umm_malloc_dep')
ext4fs_dep = subproject('lwext4').get_variable('ext2fs_dep')

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
  'include/sys/fcntl.h',
  'include/sys/mman.h',
  'include/util/list.hpp',
  'include/util/string_view.hpp',
  'klibc/stdio.h',
  'klibc/stdlib.c',
  'klibc/stdlib.h',
  'klibc/string.c',
  'klibc/string.h',
  'src/driver/pci/ide.c',
  'src/driver/pci/pci.c',
  'src/elf/elf_file_impl.cc',
  'src/elf/elf_file_impl.hpp',
  'src/elf/elf_loader.cc',
  'src/elf/elf_loader.hpp',
  'src/fs/ext4/ext4_fs.cc',
  'src/fs/ext4/ext4_fs.hpp',
  'src/fs/mem/std_fs.cc',
  'src/fs/mem/std_fs.hpp',
  'src/fs/vfs.cc',
  'src/fs/vfs.hpp',
  'src/kmain.cc',
  'src/kpanic.h',
  'src/kprintf.c',
  'src/kprintf.h',
  'src/mmu/heap.c',
  'src/mmu/heap.h',
  'src/mmu/page.cc',
  'src/mmu/page.hpp',
  'src/mmu/palloc.c',
  'src/mmu/palloc.h',
  'src/proc/proc.cc',
  'src/proc/proc.h',
  'src/proc/stack_frame.h',
  'src/runtime.cc',
  'src/screen/screen.c',
  'src/screen/screen.h',
  'src/syscall/sys_fork.cc',
  'src/syscall/sys_fork.hpp',
  'src/syscall/syscall.cc',
  'src/syscall/syscall.h',
  'src/util/string_view.cc',
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
  '-DCONFIG_USE_USER_MALLOC=1',
  '-Wno-unused-function',
]

kernel_cpp_args = [
    '-fno-exceptions',
    '-ffreestanding',
    '-fno-builtin',
    '-std=c++14',
    '-nostdinc++',
    '-fno-rtti',
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
    dependencies: [toy_boot, umm_malloc, ext4fs_dep, elf_dep],
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

executable(
  'string_view_test',
  native: true,
  sources: [ 
    'include/util/string_view.hpp',
    'src/util/string_view.cc',
    'src/util/string_view_test.cc'
  ],
  include_directories: [ './include/util' ],
  cpp_args: [ '-std=c++17', '-DTOY_HOST_TEST'],
  dependencies: [ gtest ],
)


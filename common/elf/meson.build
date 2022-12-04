

elf_dep = declare_dependency(
    include_directories : ['inc'],
    sources: [
        'inc/elf/elf.h',
        'inc/elf/elf_obj.hpp',
        'src/elf.c',
        'src/elf_obj.cc',
    ],
)

executable(
    'elf_test',
    native: true,
    sources: [
    'test/test.cc',
    ],
    cpp_args: [
        '-std=c++17',
        '-fno-rtti',
    ],
    dependencies: [ elf_dep ],
)

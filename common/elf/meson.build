

elf_dep = declare_dependency(
    include_directories : ['inc'],
    sources: [
        'inc/elf/elf.h',
        'inc/elf/elf_file.hpp',
        'src/elf_file.cc',
    ],
)

executable(
    'elf_test',
    native: true,
    include_directories : ['test'],
    sources: [
    'test/test.cc',
    'test/elf_file_host.hpp',
    'test/elf_file_host.cc',
    ],
    cpp_args: [
        '-std=c++17',
        '-fno-rtti',
    ],
    dependencies: [ elf_dep ],
)


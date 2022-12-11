

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
    'test/host_elf_obj.cc',
    'test/host_elf_obj.hpp',
    'test/test.cc',
    'test/test_common.cc',
    ],
    cpp_args: [
        '-std=c++17',
        '-fno-rtti',
    ],
    dependencies: [ elf_dep ],
)

executable(
    'host_elf_test',
    native: true,
    sources: [
    'test/host_elf_obj.cc',
    'test/host_elf_obj.hpp',
    'test/host_elf_test.cc',
    'test/test_common.cc',
    ],
    cpp_args: [
        '-std=c++17',
        '-fno-rtti',
    ],
    dependencies: [ elf_dep ],
)

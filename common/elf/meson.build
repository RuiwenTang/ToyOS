

elf_dep = declare_dependency(
    include_directories : ['inc'],
    sources: [
        'inc/elf/elf.h',
        'src/elf.c'
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
    ],
    dependencies: [ elf_dep ],
)

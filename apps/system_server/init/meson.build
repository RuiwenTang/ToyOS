

init_bin = executable(
    'init.bin',
    sources: ['main.c', 'print.S'],
    link_args: [
        '-nostdlib',
        '-T', app_link_script
    ],
    c_args: [
        '-ffreestanding',
        '-fno-builtin',
        '-nostdlib',
    ],
)

print_lib = shared_library(
    'print',
    sources: [ 'print.S' ],
    link_args: [
        '-nostdlib',
    ],
    c_args: [
        '-ffreestanding',
        '-fno-builtin',
        '-nostdlib',
    ]
)

init_dl = executable(
    'init.dl',
    sources: ['main.c'],
    link_args: [
        '-nostdlib',
        '-T', app_link_script
    ],
    c_args: [
        '-ffreestanding',
        '-fno-builtin',
        '-nostdlib',
    ],
    link_with: [ print_lib ]
)
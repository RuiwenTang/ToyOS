

init_bin = executable(
    'init.bin',
    sources: ['main.c', nasm_c.process('print.asm')],
    link_args: [
        '-nostdlib',
        '-T', app_link_script
    ],
    c_args: [
         '-ffreestanding',
        '-fno-builtin',
        '-nostdlib',
    ]
)
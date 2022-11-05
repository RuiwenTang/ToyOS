

limine_deploy = executable(
    'limine_depoly',
    native: true,
    c_args: [
        '-g2',
        '-O2',
        '-pipe',
        '-Wall',
        '-Wextra',
        '-D__USE_MINGW_ANSI_STDIO'
    ],
    sources: [
        'extern/limine-deploy.c',
    ],
    include_directories: [
        './extern'
    ]
)

limine_sys = files('extern/limine.sys')
limine_cfg = files('limine.cfg')

limine_dep = declare_dependency(
    include_directories : ['extern'],
)

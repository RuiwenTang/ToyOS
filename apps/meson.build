
# app link script
app_link_script = meson.current_source_dir() + '/link.ld'


# generate compile newlib and generate sysroot
sysroot = custom_target('sysroot',
    output: 'sysroot',
    input: [ 'gen_sysroot.py' ],
    command: [
      prog_python,
      '@INPUT@',
      meson.current_source_dir() + '/newlib',
      '@OUTPUT@',
    ]
  )

subdir('system_server')



# app link flags
app_link_flags = [
  '--sysroot=' + meson.source_root() + '/sysroot/i686-toy-elf',
  '-T', 'link.ld',
  '-z', 'noexecstack',
]

app_c_flags = [
  '--sysroot=' + meson.source_root() + '/sysroot/i686-toy-elf',
  '-I'+ meson.source_root() + '/sysroot/i686-toy-elf/include',
] 

subdir('system_server')

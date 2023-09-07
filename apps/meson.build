

# app link flags
# once patched binutils with default link flags for user-space application
# these can be removed
app_link_flags = [
  '-T', 'link.ld', # these link script is inside newlib/libgloss/toyos, and installed into `sysroot`
  '-z', 'noexecstack',
]

app_c_flags = [
] 

subdir('system_server')


init_bin = executable(
    'init.bin',
    sources: ['main.c'],
    link_args: [
        app_link_flags
    ],
    c_args: [
        app_c_flags
    ]
)

hello_cpp = executable(
  'hello_cpp.bin',
  sources: ['hello_cpp.cc'],
  link_args: [
    app_link_flags
  ],
  c_args: [
    app_c_flags
  ],
  cpp_args: [
    '-std=c++17',
  ]
)

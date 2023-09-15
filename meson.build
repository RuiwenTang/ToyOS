project('ToyOS' , 'c', 'cpp')

nasm = find_program('nasm', required: true)

prog_python = import('python').find_installation('python3')

nasm_c = generator(nasm,
  output : '@BASENAME@.o',
  arguments : [
    '-g',
    '-f', 'elf32',
    '@INPUT@',
    '-o', '@OUTPUT@'])

subdir('common/elf')
subdir('common/limine')
subdir('common/lwext4')
subdir('bootloader')
subdir('kernel')
subdir('apps')
subdir('tools')


# Generate disk image
mk_disk_img = custom_target(
  'mk_disk_img',
  output: 'ext4_disk.img',
  input: [ 'tools/gen_disk.py' ],
  command: [
    prog_python,
    '@INPUT@',
    ext4_file_tool,
    '@OUTPUT@',
    limine_deploy,
    limine_sys,
    limine_cfg,
    kernel_sys,
    init_bin,
    hello_cpp,
  ]
)


qemu_script = files('run_qemu.py')

run_qemu = run_target(
  'run_qemu',
  command: [
    prog_python, qemu_script, mk_disk_img
  ]
)

run_qemu_debug = run_target(
  'run_qemu_debug',
  command: [
    prog_python, qemu_script, mk_disk_img, '-d'
  ]
)


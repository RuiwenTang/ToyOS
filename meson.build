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
subdir('common/fatfs')
subdir('common/limine')
subdir('common/lwext4')
subdir('bootloader')
subdir('kernel')
subdir('apps')
subdir('tools')


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

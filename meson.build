project('ToyOS' , 'c', 'cpp')

nasm = find_program('nasm', required: true)

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
subdir('bootloader')
subdir('kernel')
subdir('apps')
subdir('tools')



project('ToyOS' , 'c', 'cpp')

nasm = find_program('nasm', required: true)

nasm_c = generator(nasm,
  output : '@BASENAME@.o',
  arguments : [
    '-f', 'elf32',
    '@INPUT@',
    '-o', '@OUTPUT@'])

subdir('common/fatfs')
subdir('bootloader')
subdir('kernel')
subdir('tools')



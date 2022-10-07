project('FloopyOS' , 'c', 'cpp')

subdir('bootloader')
subdir('kernel')
subdir('tools')

# run qemu

run_target(
    'qemu-run',
    depends: [floop_img],
    command: ['qemu-system-i386', '-fda', floop_img, '-monitor', 'stdio'],
)

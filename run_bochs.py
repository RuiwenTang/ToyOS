import subprocess

if __name__ == '__main__':
    subprocess.call(['echo', 'run qemu'])
    subprocess.call(['meson', 'compile', '-C', 'build', 'tools/gen_disk'])
    subprocess.call(['bochs', '-f', 'bochsrc.txt', '-q'])

import subprocess

if __name__ == '__main__':
    subprocess.call(['echo', 'run qemu'])
    ret = subprocess.call(
        ['meson', 'compile', '-C', 'build', 'tools/gen_disk'])
    if ret != 0:
        print("build failed")
        exit()
    subprocess.call(['qemu-system-i386', '-hda',
                    'build/tools/disk.img', '-monitor', 'stdio'])

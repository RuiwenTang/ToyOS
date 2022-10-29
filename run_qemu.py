import subprocess
import sys

if __name__ == '__main__':
    subprocess.call(['echo', 'run qemu'])
    ret = subprocess.call(
        ['meson', 'compile', '-C', 'build', 'tools/gen_disk'])
    if ret != 0:
        print("build failed")
        exit()
    if len(sys.argv) >= 2 and sys.argv[1] == '-d':
        subprocess.call(['qemu-system-i386', '-hda',
                         'build/tools/disk.img', '-monitor', 'stdio', '-s', '-S'])
    else:
        subprocess.call(['qemu-system-i386', '-hda',
                         'build/tools/disk.img', '-monitor', 'stdio'])

import subprocess
import sys

if __name__ == '__main__':
    subprocess.call(['echo', 'run qemu'])
    if len(sys.argv) >= 3 and sys.argv[2] == '-d':
        subprocess.call(['qemu-system-i386', '-hda',
                         sys.argv[1], '-monitor', 'stdio', '-s', '-S'])
    else:
        subprocess.call(['qemu-system-i386', '-hda',
                         sys.argv[1], '-monitor', 'stdio'])

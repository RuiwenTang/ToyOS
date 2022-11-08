import subprocess

if __name__ == '__main__':
    subprocess.call(['echo', 'run bochs'])
    ret = subprocess.call(
        ['meson', 'compile', '-C', 'build', 'tools/install_limine'])
    if ret != 0:
        print("build failed")
        exit()
    subprocess.call(['bochs', '-f', 'bochsrc.txt', '-q'])

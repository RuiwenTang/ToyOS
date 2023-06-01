import subprocess

if __name__ == '__main__':
    subprocess.call(['echo', 'run bochs'])
    subprocess.call(['bochs', '-f', 'bochsrc.txt', '-q'])

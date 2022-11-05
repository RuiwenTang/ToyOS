
import sys
import subprocess

if __name__ == '__main__':
    print(sys.argv)
    subprocess.call(['echo', 'install limine'])
    subprocess.call([sys.argv[1], sys.argv[2], ])
    pass

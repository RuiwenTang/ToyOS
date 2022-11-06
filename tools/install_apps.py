
import sys
import subprocess

if __name__ == '__main__':
    print(sys.argv)
    subprocess.call(['echo', 'run install_apps'])
    subprocess.call([sys.argv[1], sys.argv[2],
                    sys.argv[3],  # init.bin
                     ])
    pass

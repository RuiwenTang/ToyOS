
import sys
import subprocess

if __name__ == '__main__':
    print(sys.argv)
    subprocess.call(['echo', 'run gen_floppy'])
    subprocess.call([sys.argv[1], sys.argv[len(sys.argv) - 1],
                    sys.argv[2],  # limine.sys
                    sys.argv[3],  # limine.cfg
                    sys.argv[4]],  # kernel.sys
                    )
    pass


import sys
import subprocess

if __name__ == '__main__':
    subprocess.call(['echo', 'run gen_floppy'])
    subprocess.call([sys.argv[1], sys.argv[3]])
    subprocess.call(['dd', 'if=%s' % sys.argv[2], 'of=%s' %
                    sys.argv[3], 'bs=440', 'count=1', 'conv=notrunc'])
    pass

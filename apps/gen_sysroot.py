#!/usr/bin/env python3

import os
import sys
import subprocess

if __name__ == "__main__":
    print('gen sysroot')
    print(sys.argv[1:])

    subprocess.call(args=['mkdir', '-p', sys.argv[2]])
    pass

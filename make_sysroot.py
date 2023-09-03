#!/usr/bin/env python3

import os
import sys
import subprocess


def config_newlib(source_dir: str, install_dir: str):
    config_cmd = [
        os.path.join(source_dir, "configure"),
        "--prefix=" + os.path.abspath(install_dir),
        "--disable-shared",
        "--disable-libstdcxx",
        "--target=i686-toy-elf",
    ]

    return subprocess.call(config_cmd)


def build_newlib(source_dir: str, install_dir: str):
    if not os.path.isdir(source_dir):
        print("newlib source dir is not exists : %s" % source_dir)
        return 1

    if os.path.exists("build_newlib"):
        subprocess.call(["rm", "-rf", "build_newlib"])
        pass

    os.mkdir("build_newlib")

    # cd into build_newlib/
    os.chdir("build_newlib/")
    # if Mkefile is not exisit, need to config newlib
    if not os.path.exists("Makefile"):
        if config_newlib(source_dir, install_dir) != 0:
            return 1
    # build newlib
    if os.system("make") != 0:
        return 1
    # install newlib
    if os.system("make install") != 0:
        return 1
    return 0


if __name__ == "__main__":
    pwd = os.getcwd()
    source_dir = os.path.join(pwd, "apps/newlib/")
    install_dir = os.path.join(pwd, "sysroot/")
    print(source_dir)
    print(install_dir)
    # build
    os.chdir("build/")
    sys.exit(build_newlib(source_dir, install_dir))

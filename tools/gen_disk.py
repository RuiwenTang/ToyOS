from genericpath import isfile
import os
import sys
import subprocess


def create_and_format_disk(tool, disk_path):
    if os.path.isdir(disk_path):
        return 1

    if os.path.exists(disk_path):
        return 0

    # file not exists create empty img and format to ext4

    return subprocess.call([tool, "-f", disk_path, "-t", "ext4"])


def install_limine_boot(tool, disk_path):
    if not os.path.exists(disk_path):
        return 1

    return subprocess.call([tool, disk_path])


def install_file_to_disk(tool, disk_path, src_file, dst_path):
    if not os.path.exists(disk_path):
        return 1

    if not os.path.exists(src_file):
        return 1

    return subprocess.call([tool, "-f", disk_path, "-s", src_file, "-d", dst_path])


if __name__ == "__main__":
    print(sys.argv)
    tool = sys.argv[1]
    disk_path = sys.argv[2]
    limine_deploy = sys.argv[3]
    limine_sys = sys.argv[4]
    limine_cfg = sys.argv[5]
    kernel_sys = sys.argv[6]
    print("run gen_disk")

    # gen disk
    ret = create_and_format_disk(tool, disk_path)
    if ret != 0:
        print("Failed format disk")
        sys.exit(1)

    # install boot
    ret = install_limine_boot(limine_deploy, disk_path)
    if ret != 0:
        print("Failed to install boot loader")
        sys.exit(1)

    # install limine.sys
    if install_file_to_disk(tool, disk_path, limine_sys, "/boot/limine.sys") != 0:
        print("Failed to install limine.sys")
        sys.exit(1)

    # install limine.cfg
    if install_file_to_disk(tool, disk_path, limine_cfg, "/boot/limine.cfg") != 0:
        print("Failed to install limine.cfg")
        sys.exit(1)

    # install kernel
    if install_file_to_disk(tool, disk_path, kernel_sys, "/boot/kernel.sys") != 0:
        print("Failed install kernel")
        sys.exit(1)

    # subprocess.call(['echo', 'run disk'])
    # subprocess.call([sys.argv[1], sys.argv[len(sys.argv) - 1],
    #                 sys.argv[2],  # limine.sys
    #                 sys.argv[3],  # limine.cfg
    #                 sys.argv[4]],  # kernel.sys
    #                 )
    sys.exit(0)

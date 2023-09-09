#! /bin/bash

build_for_user=0
target_sysroot=""

if [ $# -eq 0 ]
then
    build_for_user=0
    echo "build toolchain without sysroot"
else
    build_for_user=1
    target_sysroot="$(pwd)/$1"

    if [ ! -d "$target_sysroot" ]
    then
        echo "target sysroot [ $target_sysroot ] not exists!"
        exit 1
    else
        echo "build toolchain with sysroot at $target_sysroot"
    fi
fi

export PREFIX="$HOME/osdev/toolchain"
export TARGET=i686-toy
export PATH="$PREFIX/bin:$PATH"

mkdir -p toolchain

cd toolchain

TOOLCHAIN_ROOT=$pwd

if [ ! -f binutils-2.39.tar.gz ]
then
    wget http://ftp.gnu.org/gnu/binutils/binutils-2.39.tar.gz
fi

if [ ! -d binutils-2.39 ]
then
    tar xvfz binutils-2.39.tar.gz
    cd binutils-2.39
    patch -p1 < ../../patchs/binutils-2.93.patch
    cd ../
fi

if [ $build_for_user == 0 ]
then
    # build with no sysroot
    if [ ! -d build_binutils ]
    then
        mkdir build_binutils
        cd build_binutils
        ../binutils-2.39/configure --target=$TARGET --prefix="$PREFIX" --disable-werror --disable-gdb
        make -j2
        make install
        cd ../
    fi
else
    # build with sysroot
    if [ ! -d build_binutils_sysroot ]
    then
        mkdir build_binutils_sysroot
        cd build_binutils_sysroot
        ../binutils-2.39/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$target_sysroot" --disable-werror --disable-gdb
        make -j2
        make install
        cd ../
    fi
fi

if [ ! -f gcc-12.2.0.tar.xz ]
then
    wget ftp://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.xz
fi

if [ ! -d gcc-12.2.0 ]
then
    tar xvf gcc-12.2.0.tar.xz
    cd gcc-12.2.0
    ./contrib/download_prerequisites
    patch -p1 < ../../patchs/gcc-12.2.0.patch
    cd ../
fi

if [ $build_for_user == 0 ]
then
    # build without sysroot
    if [ ! -d build_gcc ]
    then
        mkdir -p build_gcc
        cd build_gcc
        ../gcc-12.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --enable-shared
        make all-gcc -j2
        make all-target-libgcc
        make install-gcc
        make install-target-libgcc
    fi
else
    # build without sysroot
    if [ ! -d build_gcc_sysroot ]
    then
        mkdir -p build_gcc_sysroot
    fi
        cd build_gcc_sysroot
        ../gcc-12.2.0/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$target_sysroot" --with-newlib --disable-nls --enable-languages=c,c++ --enable-shared
        make all-gcc -j2
        make all-target-libgcc
        make install-gcc
        make install-target-libgcc
        make all-target-libstdc++-v3
        make install-target-libstdc++-v3
fi

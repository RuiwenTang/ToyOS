#! /bin/bash

export PREFIX="$HOME/osdev/toolchain"
export TARGET=i686-toy
export PATH="$PREFIX/bin:$PATH"
export CROSS_SYSROOT="$HOME/osdev/sysroot"

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
fi

if [ -d binutils-2.39 ]
then
    cd binutils-2.39
    patch -p1 < ../../patchs/binutils-2.93.patch
    cd ../
fi

if [ ! -d build_binutils ]
then
    mkdir build_binutils
    cd build_binutils
    ../binutils-2.39/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$CROSS_SYSROOT --disable-werror --disable-gdb
    make
    make install
    cd ../
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

if [ ! -d build_gcc ]
then
    mkdir -p build_gcc
    cd build_gcc
    echo $pwd
    ../gcc-12.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --enable-shared
    make all-gcc
    make all-target-libgcc
    make install-gcc
    make install-target-libgcc
fi

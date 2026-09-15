cmake_minimum_required(VERSION 3.28)

# ToyOS aarch64 - see docs/aarch64-port-arch.md
#
# Two configure trees, Serenity-style (one toolchain per tree, never mixed):
#   Build/host    - host unit tests & future host tools (preset "host")
#   Build/aarch64 - the OS itself (preset "aarch64" + cmake/aarch64-toolchain.cmake)
project(toyos VERSION 0.1.0 LANGUAGES C CXX ASM)

# Must precede add_subdirectory: the export flag only covers targets
# created after it, so a trailing set() silently skips the whole kernel
# subtree and clangd gets nothing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(TOYOS_HOST)
    message(STATUS "ToyOS host tree (unit tests & host tools)")
    # GTest via FetchContent, landed with the first copied kernel module (R1:
    # atomic.h + heap.c); host tests always compile with the same pixi-provided
    # clang as everything else in this tree, so there is no ABI mixing.
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "ToyOS aarch64 target tree")
    add_subdirectory(kernel)   # R0: arch layer skeleton
    # add_subdirectory(lib)      # R2: libsys + friends copied from ToyOS64
    # add_subdirectory(servers)  # R2: PM/VM/VFS
    # add_subdirectory(user)     # R2: init, sh, tests
endif()

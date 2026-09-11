# Cross toolchain: host (macOS arm64) -> aarch64-unknown-toyos
# LLVM-only: clang + ld.lld, both provided by the pixi environment.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOYOS_TRIPLE aarch64-unknown-toyos)

find_program(TOYOS_CLANG clang REQUIRED)
find_program(TOYOS_CLANGXX clang++ REQUIRED)

set(CMAKE_C_COMPILER "${TOYOS_CLANG}")
set(CMAKE_CXX_COMPILER "${TOYOS_CLANGXX}")

set(_toyos_target "--target=${TOYOS_TRIPLE}")
set(CMAKE_C_FLAGS_INIT "${_toyos_target}")
set(CMAKE_CXX_FLAGS_INIT "${_toyos_target}")
set(CMAKE_ASM_FLAGS_INIT "${_toyos_target}")

# Freestanding target: never link-test the compiler probe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Deliberately NOT set globally: --sysroot belongs to the userspace interface
# library, -nostdlib/-mcmodel-style flags to the kernel one. Per-target flag
# groups keep the xmake-era "flags get lost in translation" class of bug
# impossible - every flag is attached to a named interface target.

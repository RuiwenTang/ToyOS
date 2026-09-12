# Cross toolchain: host (macOS arm64) -> aarch64 kernel
# LLVM-only: clang + ld.lld, both provided by the pixi environment.
#
# The kernel compiles for the bare-metal triple aarch64-none-elf. An
# aarch64-unknown-toyos triple (the sysroot name in the blueprint) has an
# unknown OS field, which makes the clang driver fall back to the *host's*
# darwin link convention (-arch arm64, -platform_version macos …) — ld64
# arguments that ld.lld rejects. none-elf keeps the driver on the generic
# GNU path, so -T and -Wl, reach the linker untouched. The userspace sysroot
# triple is a separate R2 concern, not a rename of this one.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOYOS_TRIPLE aarch64-none-elf)

find_program(TOYOS_CLANG clang REQUIRED)
find_program(TOYOS_CLANGXX clang++ REQUIRED)

set(CMAKE_C_COMPILER "${TOYOS_CLANG}")
set(CMAKE_CXX_COMPILER "${TOYOS_CLANGXX}")

set(_toyos_target "--target=${TOYOS_TRIPLE}")
set(CMAKE_C_FLAGS_INIT "${_toyos_target}")
set(CMAKE_CXX_FLAGS_INIT "${_toyos_target}")
set(CMAKE_ASM_FLAGS_INIT "${_toyos_target}")

# Pin the ELF flavor of lld. On a darwin host -fuse-ld=lld resolves to
# ld64.lld (lld picks its driver from argv[0], and the driver passes a bare
# path); --ld-path names the ELF entry point explicitly.
find_program(TOYOS_LD ld.lld REQUIRED)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_toyos_target} --ld-path=${TOYOS_LD}")

# Freestanding target: never link-test the compiler probe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Deliberately NOT set globally: --sysroot belongs to the userspace interface
# library, -nostdlib/-mcmodel-style flags to the kernel one. Per-target flag
# groups keep the xmake-era "flags get lost in translation" class of bug
# impossible - every flag is attached to a named interface target.

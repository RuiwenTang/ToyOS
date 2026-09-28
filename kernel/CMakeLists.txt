# kernel — the aarch64 kernel image
#
# Flags live on one interface target; the kernel is freestanding and never
# touches the userspace sysroot (that split comes with lib/ in R2).

add_library(toyos_kernel_flags INTERFACE)
target_compile_options(toyos_kernel_flags INTERFACE
    -ffreestanding
    -fno-pic
    -fno-stack-protector
    -fno-builtin
    -fno-asynchronous-unwind-tables
    -mgeneral-regs-only      # no FP/SIMD until fpu.c enables CPACR (R1)
    -D__TOYOS_KERNEL__       # heap.c dual (kernel/host) paths select the kernel one
    -Wall -Wextra
    -O2 -g)
target_include_directories(toyos_kernel_flags INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Kernel
    main.c
    fdt.c
    string.c
    arch/aarch64/entry.S
    arch/aarch64/traps.S
    arch/aarch64/traps.c
    arch/aarch64/gicv3.c
    arch/aarch64/arch_timer.c
    arch/aarch64/serial_pl011.c
    arch/aarch64/bootmmu.c
    arch/aarch64/paging.c
    arch/aarch64/cache.c
    arch/aarch64/psci.c
    arch/aarch64/smp.c
    arch/aarch64/switch.S
    sched/percpu.c
    sched/spinlock.c
    sched/sched.c
    sched/thread.c
    sched/sleep.c
    intr/timer.c
    mm/heap.c
    mm/heap_provider.c
    mm/bitmap.c
    mm/memmap.c
    mm/pmm.c
    mm/kstack.c)
set_target_properties(Kernel PROPERTIES OUTPUT_NAME Kernel SUFFIX ".elf")
target_link_libraries(Kernel PRIVATE toyos_kernel_flags)
target_link_options(Kernel PRIVATE
    -nostdlib
    -static
    -T ${CMAKE_CURRENT_SOURCE_DIR}/arch/aarch64/linker.ld
    -Wl,--no-undefined)

# Raw binary + arm64 boot header. The resulting Image is what both boot
# paths load (QEMU -kernel and U-Boot booti) — one image, two loaders.
# An OUTPUT rule (not POST_BUILD): editing mkimage.py must regenerate the
# Image too, or a stale header silently ships (the magic bug bit exactly
# that way).
find_program(TOYOS_OBJCOPY llvm-objcopy REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(TOYOS_MKIMAGE ${CMAKE_SOURCE_DIR}/scripts/boot/mkimage.py)
set(TOYOS_IMAGE_DIR ${CMAKE_BINARY_DIR}/kernel)
add_custom_command(
    OUTPUT ${TOYOS_IMAGE_DIR}/Image
    COMMAND ${TOYOS_OBJCOPY} -O binary $<TARGET_FILE:Kernel>
            ${TOYOS_IMAGE_DIR}/Kernel.img
    COMMAND Python3::Interpreter ${TOYOS_MKIMAGE}
            ${TOYOS_IMAGE_DIR}/Kernel.img ${TOYOS_IMAGE_DIR}/Image
    DEPENDS Kernel ${TOYOS_MKIMAGE}
    COMMENT "mkimage: Image (arm64 boot header + Kernel.img)"
    VERBATIM)
add_custom_target(Image ALL DEPENDS ${TOYOS_IMAGE_DIR}/Image)

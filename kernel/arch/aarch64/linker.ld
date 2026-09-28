/*
 * linker.ld — ToyOS aarch64 kernel memory layout
 *
 * R2.2: the kernel links at the high-half image region (TTBR1 L0 slot
 * 511, L1 #510 — see kva.h). The Image still LOADS at RAM base + 0x80000
 * (arm64 text_offset convention; both loaders honour the Image header,
 * not the link VA), and runs there under the identity trampoline until
 * bootmmu turns on both TTBRs and entry.S jumps to the link VA. All C
 * references are ADRP-relative, which resolve against the running PC —
 * valid at either base — so only hand-written asm needs the load/link
 * delta (entry.S owns it).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

ENTRY(_start)

SECTIONS
{
    /* Link base = KERNEL_IMAGE_BASE + 0x40: the 64-byte arm64 boot header
     * (attached by scripts/boot/mkimage.py) occupies the image's first
     * page prefix, so code links at +0x40 to keep file offset == VA -
     * base under both loaders.
     *
     * .text.boot must be its own output section: folding it into .text
     * let the vector table's 2 KiB alignment drag the whole section (and
     * _start with it) off its link position — every cross-page ADRP then
     * addressed memory below where the code actually ran. */
    . = 0xFFFFFFFF80000000;
    __kernel_start = .;   /* VA; memmap reserves the PA extent via kimage_offset */
    . = 0xFFFFFFFF80000040;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    .vectors : ALIGN(2048) {
        KEEP(*(.vectors))
    }

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(16);
    __bss_end = .;

    /* Boot stack for the BSP (per-CPU stacks arrive with SMP in R1). */
    .boot_stack (NOLOAD) : {
        . = . + 0x10000;     /* 64 KiB */
    }
    . = ALIGN(16);
    __stack_top = .;

    /* Whole-image extent (header slot through boot stack; .bss and the AP
     * boot stacks live inside): memmap_init reserves this from the pmm. */
    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.attributes)
    }
}

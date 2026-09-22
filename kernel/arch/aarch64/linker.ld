/*
 * linker.ld — ToyOS aarch64 kernel memory layout
 *
 * Linked (and loaded) at RAM base + 0x80000, following the Linux arm64
 * text_offset convention: 0x40000000 (QEMU virt RAM base, 2 MiB-aligned)
 * + 0x80000 = 0x40080000. Both boot paths place the image here — QEMU's
 * -kernel loads the ELF at its program headers, U-Boot's booti honours the
 * image header's offset field — so the same binary runs either way.
 *
 * R0 runs identity-mapped at the physical address; the higher-half
 * TTBR0/TTBR1 split arrives with paging in R2.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

ENTRY(_start)

SECTIONS
{
    /* The 64-byte arm64 boot header (attached by scripts/boot/mkimage.py)
     * occupies [0x40080000, 0x40080040); code links at +0x40 so that
     * run-time PC == link-time PC under both loaders (QEMU places the
     * whole Image at RAM base + text_offset; the ELF's program headers
     * agree).
     *
     * .text.boot must be its own output section: folding it into .text
     * let the vector table's 2 KiB alignment drag the whole section (and
     * _start with it) up to 0x40080800 — every cross-page ADRP then
     * addressed memory 0x7C0 below where the code actually ran. */
    . = 0x40080000;
    __kernel_start = .;   /* includes the header slot — memmap reserves from here */
    . = 0x40080040;

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

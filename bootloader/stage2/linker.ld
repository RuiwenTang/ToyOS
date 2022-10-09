
ENTRY(entry)
OUTPUT_FORMAT("binary")
phys = 0x00008000;

SECTIONS
{
    . = phys;

    .entry              : { *(.entry)   }
    .text               : { *(.text)    }
    .data               : { *(.data)    }
    .rodata             : { *(.rodata)  }
    .bss                : { *(.bss)     }

}
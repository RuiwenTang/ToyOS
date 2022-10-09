bits 16

section .entry

jmp entry

extern stage2_main
global entry
entry:
    mov ax, 0x9000
    mov si, ax
    mov byte [si], 3
    jmp $
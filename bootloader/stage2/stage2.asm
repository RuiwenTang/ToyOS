bits 16

section .entry

extern stage2_main
global entry
entry:
    push 1
    push 2
    call stage2_main
    jmp $
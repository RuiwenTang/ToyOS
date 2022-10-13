

section .bss
align 16
stack_bottom:
resb 16384      ; 16kb
stack_top:


section .text

global start
extern kernel_main
start:
  mov esp, stack_top
  call kernel_main

  cli
.hang:
  hlt
  jmp .hang

.end:


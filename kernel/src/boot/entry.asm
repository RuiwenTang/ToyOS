

section .bss
align 16
stack_bottom:
resb 16384      ; 16kb
global stack_top
stack_top:


section .text

global start
extern kernel_main
start:
  cli
  mov ebx, [esp + 4]
  mov esp, stack_top
  push esp
  push ebx
  call kernel_main

  cli
.hang:
  hlt
  jmp .hang

.end:


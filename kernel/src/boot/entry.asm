
; Declare constants for the multiboot header.
MBALIGN   equ  1 << 0                         ; align loaded modules on page boundaries
MEMINFO   equ  1 << 1                         ; provide memory map
VIDEOINFO equ  4                          ; privide framebuffer video info
FLAGS     equ  MBALIGN | MEMINFO | VIDEOINFO  ; this is the Multiboot 'flag' field
MAGIC     equ  0x1BADB002                     ; 'magic number' lets bootloader find the header
CHECKSUM  equ -(MAGIC + FLAGS)                ; checksum of above, to prove we are multiboot

section .multiboot
align 4
  dd MAGIC
  dd FLAGS
  dd CHECKSUM
  dd 0
  dd 0
  dd 0
  dd 0
  dd 0
  dd 0
  dd 1024
  dd 768
  dd 32
section .text

global start
extern kernel_main
start:
  cli
  mov esp, stack_top
  push ebx
  push eax
  mov eax, esp
  add eax, 8
  push esp
  call kernel_main

  cli
.hang:
  hlt
  jmp .hang

.end:


section .bss
align 16
stack_bottom:
resb 16384      ; 16kb
global stack_top
stack_top:

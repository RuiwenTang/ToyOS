bits 16


global start

start:
   mov ah, 0x0E
   mov al, 'A'
   mov bh, 0
   int 0x10
   mov al, 'B'
   int 0x10
   jmp $



; void k_print(char* str)
global k_print
k_print:

  mov eax, 1

  int 0x80

  ret

// void k_print(char* str)
.global k_print
.type k_print, @function
k_print:
  pushl %ebx
  movl $1, %eax
  movl 8(%esp), %ebx
  int $0x80

  popl %ebx
  ret
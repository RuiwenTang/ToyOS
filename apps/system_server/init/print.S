
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

// void* k_mmap(int size)
.global k_mmap
.type k_mmap, @function
k_mmap:
  pushl %ebx
  pushl %ecx
  pushl %edx
  movl $2, %eax
  movl 16(%esp), %ebx // size
  movl $0, %ecx       // p_addr
  movl $0x20, %edx    // flags

  int $0x80

  popl %edx
  popl %ecx
  popl %ebx
  ret
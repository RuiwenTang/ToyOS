[bits 32]

; void page_load_directory(void* pdr);
global page_load_directory
page_load_directory:
  mov eax, [esp + 4]
  mov cr3, eax
  ret

; void page_enable();
global page_enable
page_enable:
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax
  ret


extern kernel_start
extern kernel_end

global get_kernel_start
get_kernel_start:
  mov eax, kernel_start
  ret

global get_kernel_end
get_kernel_end:
  mov eax, kernel_end
  ret

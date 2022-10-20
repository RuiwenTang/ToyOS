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


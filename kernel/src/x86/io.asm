
[bits 32]

; void x86_out8(uint16_t port, uint8_t value);
global x86_out8
x86_out8:
  mov dx, [esp + 4]   ; port number
  mov al, [esp + 8]   ; value

  out dx, al
  ret

; void x86_out16(uint16_t port, uint16_t value)
global x86_out16
x86_out16:
  mov dx, [esp + 4]   ; port number
  mov ax, [esp + 8]   ; value

  out dx, ax
  ret

; void x86_out32(uint16_t port, uint32_t value)
global x86_out32
x86_out32:
  mov dx, [esp + 4]   ; port number
  mov eax, [esp + 8]  ; value
  out dx, eax
  ret

; uint8_t x86_in8(uint16_t port);
global x86_in8
x86_in8:
  mov dx, [esp + 4]
  xor eax, eax
  in al, dx
  ret

; uint16_t x86_in16(uint16_t port)
global x86_in16
x86_in16:
  mov dx, [esp + 4]
  xor eax, eax
  in ax, dx
  ret

; uint32_t x86_in32(uint16_t port)
global x86_in32
x86_in32:
  mov dx, [esp + 4]
  xor eax, eax
  in eax, dx
  ret

;void x86_rep_in16(uint16_t port, uint32_t times, uint32_t addr)
global x86_rep_in16
x86_rep_in16:
  push edx
  push ecx
  push edi

  xor edx, edx
  mov dx, [esp + 16]
  mov ecx, [esp + 20]
  mov edi, [esp + 24]

  rep insw
  
  pop edi
  pop ecx
  pop edx

  ret

; void x86_rep_out16(uint16_t port, uint32_t times, uint32_t addr)
global x86_rep_out16
x86_rep_out16:
  push edx
  push ecx
  push esi

  xor edx, edx
  mov dx, [esp + 16]
  mov ecx, [esp + 20]
  mov esi, [esp + 24]

  rep outsw
  
  pop esi
  pop ecx
  pop edx
  rep

; void x86_enable_interrupt();
global x86_enable_interrupt
x86_enable_interrupt:
  sti
  ret

; void x86_disable_interrupt();
global x86_disable_interrupt
x86_disable_interrupt:
  cli
  ret

; void x86_iowait();
global x86_iowait
x86_iowait:
  nop
  nop
  ret

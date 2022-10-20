
[bits 32]

; void x86_outb(uint16_t port, uint8_t value);
global x86_outb
x86_outb:
  mov dx, [esp + 4]   ; port number
  mov al, [esp + 8]   ; value

  out dx, al
  ret

; uint8_t x86_inb(uint16_t port);

global x86_inb
x86_inb:
  mov dx, [esp + 4]
  xor eax, eax
  in al, dx
  ret

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

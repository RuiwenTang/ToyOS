
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    jmp 0x08:gdt_flush.flush2
.flush2:
    ret

global tss_flush
tss_flush:
  xor eax, eax
  mov eax, [esp + 4]
  ltr ax
  ret
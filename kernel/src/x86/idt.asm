

; void idt_flush(IDT_PTR* ptr);
global idt_flush
idt_flush:
  mov eax, [esp + 4]
  lidt [eax]
  ret
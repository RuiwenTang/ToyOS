bits 32

%macro x86_EnterRealMode 0
    [bits 32]
    jmp word 18h:.pmode16         ; 1 - jump to 16-bit protected mode segment

.pmode16:
    [bits 16]
    ; 2 - disable protected mode bit in cr0
    mov eax, cr0
    and al, ~1
    mov cr0, eax

    ; 3 - jump to real mode
    jmp word 00h:.rmode

.rmode:
    ; 4 - setup segments
    mov ax, 0
    mov ds, ax
    mov ss, ax

    ; 5 - enable interrupts
    sti

%endmacro


%macro x86_EnterProtectedMode 0
    cli

    ; 4 - set protection enable flag in CR0
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; 5 - far jump into protected mode
    jmp dword 08h:.pmode


.pmode:
    ; we are now in protected mode!
    [bits 32]
    
    ; 6 - setup segment registers
    mov ax, 0x10
    mov ds, ax
    mov ss, ax

%endmacro

section .text

global bios_disk_read
bios_disk_read:
[bits 32]
    push ebp        ; save current stack
    mov ebp, esp    ;

    x86_EnterRealMode
[bits 16]
    ;save regs
    push ebx
    push si

    mov si, read_param

    mov dl, [bp + 8]        ; driver number
    mov eax, [bp + 12]      ; lba address
    mov ebx,  [bp + 16]

    mov byte [si], 0x10     ; fixed value
    mov byte [si + 1], 0    ; fixed value
    mov word [si + 2], 1    ; only read one sector
    mov word [si + 4], bx   ; addr
    mov word [si + 6], 0    ; addr segment
    mov dword [si + 8], eax ; lba address
    mov dword [si + 12], 0  ; only 32bit lba address for now

    clc
    mov al, 0
    mov ah, 0x42
    int 0x13

    jc .read_failed
    jmp .read_success

.read_failed:
    mov eax, 1
    jmp .save_return
.read_success:
    mov eax, 0
    jmp .save_return

.save_return:
    pop si
    pop ebx

    push eax

    x86_EnterProtectedMode

    pop eax

    mov esp, ebp
    pop ebp
    ret

section .data
; read disk parameter
read_param:
    resb 16


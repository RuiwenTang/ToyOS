bits 16

; macros
%define ENDL 0x0D, 0x0A
%define KbdControllerDataPort                0x60
%define KbdControllerCommandPort             0x64
%define KbdControllerDisableKeyboard         0xAD
%define KbdControllerEnableKeyboard          0xAE
%define KbdControllerReadCtrlOutputPort      0xD0
%define KbdControllerWriteCtrlOutputPort     0xD1

section .entry

jmp entry

extern stage2_main
global entry
entry:
    mov byte [boot_disk_driver], dl ;save boot driver

    ; setup stack
    mov sp, 0xFFFF

    cli                     ; disable interrupt
    ; switch to protect mode
    call enable_a20         ; 2 - Enable A20 gate
    call load_gdt           ; 3 - Load GDT

    ; 4 - set protection enable flag in CR0
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; 5 - far jump into protected mode
    jmp dword 0x08:pmode
    
.done:
    jmp $

enable_a20:
    call check_a20
    cmp ax, 1
    je enable_a20.success      ; already enabled

    ; disable keyboard
    call a20_wait_input
    mov al, KbdControllerDisableKeyboard
    out KbdControllerCommandPort, al

    ; read control output port
    call a20_wait_input
    mov al, KbdControllerReadCtrlOutputPort
    out KbdControllerCommandPort, al

    call a20_wait_input
    in al, KbdControllerDataPort
    push ax

    ; write control output port
    call a20_wait_input
    mov al, KbdControllerWriteCtrlOutputPort
    out KbdControllerCommandPort, al

    call a20_wait_input
    pop ax
    or al, 2
    out KbdControllerDataPort, al

    ; enable keyboard
    call a20_wait_input
    mov al, KbdControllerEnableKeyboard
    out KbdControllerCommandPort, al

    call a20_wait_input

    ; check if a20 is enabled
    call check_a20
    cmp ax, 1
    je enable_a20.success

.failed:
    mov si, msg_a20_failed
    call print
    ret
.success:
    mov si, msg_a20_enabled
    call print
    ret

; https://wiki.osdev.org/A20_Line
; check if a20 is enabled
;
; Returns: 0 in ax if the a20 line is disabled (memory wraps around)
;          1 in ax if the a20 line is enabled (memory does not wrap around)
check_a20:
    pushf
    push ds
    push es
    push di
    push si

    xor ax, ax  ; ax = 0
    mov es, ax
 
    not ax      ; ax = 0xFFFF
    mov ds, ax

    mov di, 0x0500
    mov si, 0x0510

    mov al, byte [es:di]
    push ax
 
    mov al, byte [ds:si]
    push ax
 
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
 
    cmp byte [es:di], 0xFF

    pop ax
    mov byte [ds:si], al

    pop ax
    mov byte [es:di], al

    mov ax, 0
    je check_a20.done
    
    mov ax, 1

.done:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret


; Prints a string to the screen
; Params:
; - ds:si points to string
;
print:
    ; save registers
    push si
    push ax
    push bx

.loop:
    lodsb      ; load DS:SI into AL
    or al, al   ; check if al is 0
    jz print.done

    mov ah, 0x0E
    mov bl, 0x04
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    ; restore registers
    pop bx
    pop ax
    pop si
    ret

a20_wait_input:
    ; wait until status bit 2 (input buffer) is 0
    ; by reading from command port, we read status byte
    in al, KbdControllerCommandPort
    test al, 2
    jnz a20_wait_input
    ret

load_gdt:
    lgdt [stage2_gdt_desc]
    ret

section .data
; save boot driver number
boot_disk_driver:
    db 0

; GDT for stage2, only
; kernel will reset ged in future
stage2_gdt:
    ; first always NULL
    dq  0
    ; 32 bits code segment 
    dw 0xFFFF                   ; limit (bits 0-15)  0xFFFFF for full 32-bit range
    dw 0x0                      ; base (bits 0-15)   0x0
    db 0x0                      ; base (bits 16-23)  0x0
    db 10011010b                ; access (present, ring 0, code segment, executable, direction 0, readable)
    db 11001111b             ; granularity (4k pages, 32-bit pmode) + limit (bits 16-19)
    db 0                        ; base high
    ; 32-bit data segment
    dw 0xFFFF                   ; limit (bits 0-15)  0xFFFFF for full 32-bit range
    dw 0x0                      ; base (bits 0-15)   0x0
    db 0x0                      ; base (bits 16-23)  0x0
    db 10010010b                ; access (present, ring 0, data segment, executable, direction 0, writable)
    db 11001111b                ; granularity (4k pages, 32-bit pmode) + limit (bits 16-19)
    db 0                        ; base high

stage2_gdt_desc:  
    dw $ - stage2_gdt - 1    ; limit = size of GDT
    dd stage2_gdt               ; base

section .rodata

msg_a20_enabled:
    db "A20 is enabled!", ENDL, 0
msg_a20_failed:
    db "A20 enable failed!", ENDL, 0

section .text
pmode:
    [bits 32]
    ; 6 - setup segment registers
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov gs, ax
    mov fs, ax
    mov eax, 1
    push eax
    mov eax, 2
    push eax
    call stage2_main
    jmp $

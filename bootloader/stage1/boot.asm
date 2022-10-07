bits 16

; flat boot sector used to load kernel kernel
section .entry
global start
; entry point of this stage
start:
    ; setup data segments
    mov ax, 0           ; can't set ds/es directly
    mov ds, ax
    mov es, ax
    mov ss, ax
    ; setup stack
    mov sp, 0x7c00      ; stack grows downwards from where we are loaded in memory

    mov si, hello_world_msg
    call print

    jmp $


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
    jz .done

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

section .rodata
; read only data
hello_world_msg:
    db  "Hello World!", 0

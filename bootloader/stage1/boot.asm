bits 16

; macros
%define ENDL 0x0D, 0x0A
; 0x7c00 + 0x1A4
%define STAGE2_SIZE_LOC 0x7DA4

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

    mov ah, 0x41
    mov bx, 0x55AA
    stc
    int 0x13
    jc start.lba_no_support                ; no support

    ; just for debug try read 1 sector
    mov bx, 0x8000
    mov eax, 0x1
    mov dword ecx, [STAGE2_SIZE_LOC]
    clc
    call read_sectors
    jc start.read_failed

    jmp 0x8000
.done:
    jmp $

.lba_no_support:
    mov si, lba_no_support
    call print
    jmp start.done

.read_failed:
    mov si, read_disk_failed
    call print
    jmp start.done


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

; AX = start sector number
; CX = sector count to read
; DL = Drive number
; ES = Buffer segment
; BX = Buffer offset
; OUT:
; Carry if error
read_sectors:
    mov si, read_param
    mov byte [si], 0x10
    mov byte [si + 1], 0
    mov word [si + 2], cx
    mov word [si + 4], bx
    mov word [si + 6], es
    mov dword [si + 8], eax
    mov dword [si + 12], 0

    mov al, 0
    mov ah, 0x42
    int 0x13
    ret

section .data
; read write data
; offset: 0 -> size of DAP (set this to 10h)
; offset: 1 -> unused set zero
; offset: 2 -> 2 bytes. number of sector to read
; offset: 4 -> 4 bytes. segment:offset pointer to the memory buffer to which sectors will be transferred
; offset: 8 -> 8 bytes. absolute number of the start of the sectors to be read
read_param:
    resb 16

section .rodata
; read only data
hello_world_msg:
    db  "Hello World!", ENDL, 0

lba_no_support:
    db "int 0x13 not support LBA!", ENDL, 0

read_disk_failed:
    db "read disk failed!", ENDL, 0

[bits 32]

; ASM const
P_STACKBASE     equ             0
GSREG           equ	            P_STACKBASE
FSREG           equ             GSREG + 4
ESREG           equ             FSREG + 4
DSREG           equ             ESREG + 4
EDIREG          equ             DSREG + 4
ESIREG          equ             EDIREG + 4
EBPREG          equ             ESIREG + 4
KERNELESPREG    equ             EBPREG + 4
EBXREG          equ             KERNELESPREG + 4
EDXREG          equ             EBXREG + 4
ECXREG          equ             EDXREG + 4
EAXREG          equ             ECXREG + 4
INTNUM          equ             EAXREG + 4
ERRNUM          equ             INTNUM + 4
EIPREG          equ             ERRNUM + 4
CSREG           equ             EIPREG + 4
EFLAGSREG       equ             CSREG + 4
ESPREG          equ             EFLAGSREG + 4
SSREG           equ             ESPREG + 4
P_STACKTOP      equ             SSREG + 4

TSS3_S_SP0 equ 4
; defined in proc.c
extern current_proc
; defined in gdt.c
extern g_tss

global proc_restart
proc_restart:
  mov esp, [current_proc]
  lea eax, [esp + P_STACKTOP]
  mov dword [g_tss + TSS3_S_SP0], eax

  pop gs
  pop fs
  pop es
  pop ds
  popad
  add esp, 8    ; skip interrupt and error code
  iret
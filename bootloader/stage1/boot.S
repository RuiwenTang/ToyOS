
.code16

.global start
// section to start code
.section .fsjump

JMP start
  NOP

// section for fat headers
.section .fsheaders

.byte "floppyos"    // name of first sector, 8 bytes
.word 512           // size of one sector, must be 512
.byte 1             // size of cluster must be 1
.word 1             // start sector of fat
.byte 2             // count of fat must be 2
.word 224           // size of root directory
.word 2880          // size of this disk must be 2880
.byte 0xF0          // type of this disk must be 0xf0
.word 9             // length of FAT must be 9
.word 18            // how many sector in one track, must be 18
.word 2             // must be 2
.long 0             // no need format
.long 2880          // rewrite size of disk
.byte 0
.byte 0
.byte 0x29
.long 0xffffffff
.byte "floopy os  "
.byte "FAT12   "


start:
  HLT

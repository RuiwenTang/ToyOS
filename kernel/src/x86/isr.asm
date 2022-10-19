
; system iterrupt router

; all interrupt, cpu pushes to stack:
; ss, esp, eflags, cs, eip

extern c_isr_handler

%macro ISR_NO_ERROR_CODE 1
global x86_isr%1
x86_isr%1:
  push 0                ; push dummy error code
  push %1               ; push interrupt number
  jmp isr_common        ; use jump to not change esp and stack
%endmacro

%macro ISR_ERROR_CODE 1
global x86_isr%1
x86_isr%1:
  ; error code is pushed by cpu
  push %1               ; push interrup number
  jmp isr_common        ; use jump to not change esp and stack
%endmacro


isr_common:
  pusha                 ; pushes all registers in orders:
                        ; eax
                        ; ecx
                        ; edx
                        ; ebx
                        ; esp
                        ; ebp
                        ; esi
                        ; edi
  xor eax, eax
  mov ax, ds
  push eax              ; save ds

  mov ax, 0x10          ; use kernel data segment
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  push esp              ; pass pointer to stack and pass to c function
  call c_isr_handler
  add esp, 4

  ; restore data segment
  pop eax
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  popa
  add esp, 8
  iret


ISR_NO_ERROR_CODE 0
ISR_NO_ERROR_CODE 1
ISR_NO_ERROR_CODE 2
ISR_NO_ERROR_CODE 3
ISR_NO_ERROR_CODE 4
ISR_NO_ERROR_CODE 5
ISR_NO_ERROR_CODE 6
ISR_NO_ERROR_CODE 7
ISR_ERROR_CODE 8
ISR_NO_ERROR_CODE 9
ISR_ERROR_CODE 10
ISR_ERROR_CODE 11
ISR_ERROR_CODE 12
ISR_ERROR_CODE 13
ISR_ERROR_CODE 14
ISR_NO_ERROR_CODE 15
ISR_NO_ERROR_CODE 16
ISR_ERROR_CODE 17
ISR_NO_ERROR_CODE 18
ISR_NO_ERROR_CODE 19
ISR_NO_ERROR_CODE 20
ISR_ERROR_CODE 21
ISR_NO_ERROR_CODE 22
ISR_NO_ERROR_CODE 23
ISR_NO_ERROR_CODE 24
ISR_NO_ERROR_CODE 25
ISR_NO_ERROR_CODE 26
ISR_NO_ERROR_CODE 27
ISR_NO_ERROR_CODE 28
ISR_NO_ERROR_CODE 29
ISR_NO_ERROR_CODE 30
ISR_NO_ERROR_CODE 31
ISR_NO_ERROR_CODE 32
ISR_NO_ERROR_CODE 33
ISR_NO_ERROR_CODE 34
ISR_NO_ERROR_CODE 35
ISR_NO_ERROR_CODE 36
ISR_NO_ERROR_CODE 37
ISR_NO_ERROR_CODE 38
ISR_NO_ERROR_CODE 39
ISR_NO_ERROR_CODE 40
ISR_NO_ERROR_CODE 41
ISR_NO_ERROR_CODE 42
ISR_NO_ERROR_CODE 43
ISR_NO_ERROR_CODE 44
ISR_NO_ERROR_CODE 45
ISR_NO_ERROR_CODE 46
ISR_NO_ERROR_CODE 47
ISR_NO_ERROR_CODE 48
ISR_NO_ERROR_CODE 49
ISR_NO_ERROR_CODE 50
ISR_NO_ERROR_CODE 51
ISR_NO_ERROR_CODE 52
ISR_NO_ERROR_CODE 53
ISR_NO_ERROR_CODE 54
ISR_NO_ERROR_CODE 55
ISR_NO_ERROR_CODE 56
ISR_NO_ERROR_CODE 57
ISR_NO_ERROR_CODE 58
ISR_NO_ERROR_CODE 59
ISR_NO_ERROR_CODE 60
ISR_NO_ERROR_CODE 61
ISR_NO_ERROR_CODE 62
ISR_NO_ERROR_CODE 63
ISR_NO_ERROR_CODE 64
ISR_NO_ERROR_CODE 65
ISR_NO_ERROR_CODE 66
ISR_NO_ERROR_CODE 67
ISR_NO_ERROR_CODE 68
ISR_NO_ERROR_CODE 69
ISR_NO_ERROR_CODE 70
ISR_NO_ERROR_CODE 71
ISR_NO_ERROR_CODE 72
ISR_NO_ERROR_CODE 73
ISR_NO_ERROR_CODE 74
ISR_NO_ERROR_CODE 75
ISR_NO_ERROR_CODE 76
ISR_NO_ERROR_CODE 77
ISR_NO_ERROR_CODE 78
ISR_NO_ERROR_CODE 79
ISR_NO_ERROR_CODE 80
ISR_NO_ERROR_CODE 81
ISR_NO_ERROR_CODE 82
ISR_NO_ERROR_CODE 83
ISR_NO_ERROR_CODE 84
ISR_NO_ERROR_CODE 85
ISR_NO_ERROR_CODE 86
ISR_NO_ERROR_CODE 87
ISR_NO_ERROR_CODE 88
ISR_NO_ERROR_CODE 89
ISR_NO_ERROR_CODE 90
ISR_NO_ERROR_CODE 91
ISR_NO_ERROR_CODE 92
ISR_NO_ERROR_CODE 93
ISR_NO_ERROR_CODE 94
ISR_NO_ERROR_CODE 95
ISR_NO_ERROR_CODE 96
ISR_NO_ERROR_CODE 97
ISR_NO_ERROR_CODE 98
ISR_NO_ERROR_CODE 99
ISR_NO_ERROR_CODE 100
ISR_NO_ERROR_CODE 101
ISR_NO_ERROR_CODE 102
ISR_NO_ERROR_CODE 103
ISR_NO_ERROR_CODE 104
ISR_NO_ERROR_CODE 105
ISR_NO_ERROR_CODE 106
ISR_NO_ERROR_CODE 107
ISR_NO_ERROR_CODE 108
ISR_NO_ERROR_CODE 109
ISR_NO_ERROR_CODE 110
ISR_NO_ERROR_CODE 111
ISR_NO_ERROR_CODE 112
ISR_NO_ERROR_CODE 113
ISR_NO_ERROR_CODE 114
ISR_NO_ERROR_CODE 115
ISR_NO_ERROR_CODE 116
ISR_NO_ERROR_CODE 117
ISR_NO_ERROR_CODE 118
ISR_NO_ERROR_CODE 119
ISR_NO_ERROR_CODE 120
ISR_NO_ERROR_CODE 121
ISR_NO_ERROR_CODE 122
ISR_NO_ERROR_CODE 123
ISR_NO_ERROR_CODE 124
ISR_NO_ERROR_CODE 125
ISR_NO_ERROR_CODE 126
ISR_NO_ERROR_CODE 127
ISR_NO_ERROR_CODE 128
ISR_NO_ERROR_CODE 129
ISR_NO_ERROR_CODE 130
ISR_NO_ERROR_CODE 131
ISR_NO_ERROR_CODE 132
ISR_NO_ERROR_CODE 133
ISR_NO_ERROR_CODE 134
ISR_NO_ERROR_CODE 135
ISR_NO_ERROR_CODE 136
ISR_NO_ERROR_CODE 137
ISR_NO_ERROR_CODE 138
ISR_NO_ERROR_CODE 139
ISR_NO_ERROR_CODE 140
ISR_NO_ERROR_CODE 141
ISR_NO_ERROR_CODE 142
ISR_NO_ERROR_CODE 143
ISR_NO_ERROR_CODE 144
ISR_NO_ERROR_CODE 145
ISR_NO_ERROR_CODE 146
ISR_NO_ERROR_CODE 147
ISR_NO_ERROR_CODE 148
ISR_NO_ERROR_CODE 149
ISR_NO_ERROR_CODE 150
ISR_NO_ERROR_CODE 151
ISR_NO_ERROR_CODE 152
ISR_NO_ERROR_CODE 153
ISR_NO_ERROR_CODE 154
ISR_NO_ERROR_CODE 155
ISR_NO_ERROR_CODE 156
ISR_NO_ERROR_CODE 157
ISR_NO_ERROR_CODE 158
ISR_NO_ERROR_CODE 159
ISR_NO_ERROR_CODE 160
ISR_NO_ERROR_CODE 161
ISR_NO_ERROR_CODE 162
ISR_NO_ERROR_CODE 163
ISR_NO_ERROR_CODE 164
ISR_NO_ERROR_CODE 165
ISR_NO_ERROR_CODE 166
ISR_NO_ERROR_CODE 167
ISR_NO_ERROR_CODE 168
ISR_NO_ERROR_CODE 169
ISR_NO_ERROR_CODE 170
ISR_NO_ERROR_CODE 171
ISR_NO_ERROR_CODE 172
ISR_NO_ERROR_CODE 173
ISR_NO_ERROR_CODE 174
ISR_NO_ERROR_CODE 175
ISR_NO_ERROR_CODE 176
ISR_NO_ERROR_CODE 177
ISR_NO_ERROR_CODE 178
ISR_NO_ERROR_CODE 179
ISR_NO_ERROR_CODE 180
ISR_NO_ERROR_CODE 181
ISR_NO_ERROR_CODE 182
ISR_NO_ERROR_CODE 183
ISR_NO_ERROR_CODE 184
ISR_NO_ERROR_CODE 185
ISR_NO_ERROR_CODE 186
ISR_NO_ERROR_CODE 187
ISR_NO_ERROR_CODE 188
ISR_NO_ERROR_CODE 189
ISR_NO_ERROR_CODE 190
ISR_NO_ERROR_CODE 191
ISR_NO_ERROR_CODE 192
ISR_NO_ERROR_CODE 193
ISR_NO_ERROR_CODE 194
ISR_NO_ERROR_CODE 195
ISR_NO_ERROR_CODE 196
ISR_NO_ERROR_CODE 197
ISR_NO_ERROR_CODE 198
ISR_NO_ERROR_CODE 199
ISR_NO_ERROR_CODE 200
ISR_NO_ERROR_CODE 201
ISR_NO_ERROR_CODE 202
ISR_NO_ERROR_CODE 203
ISR_NO_ERROR_CODE 204
ISR_NO_ERROR_CODE 205
ISR_NO_ERROR_CODE 206
ISR_NO_ERROR_CODE 207
ISR_NO_ERROR_CODE 208
ISR_NO_ERROR_CODE 209
ISR_NO_ERROR_CODE 210
ISR_NO_ERROR_CODE 211
ISR_NO_ERROR_CODE 212
ISR_NO_ERROR_CODE 213
ISR_NO_ERROR_CODE 214
ISR_NO_ERROR_CODE 215
ISR_NO_ERROR_CODE 216
ISR_NO_ERROR_CODE 217
ISR_NO_ERROR_CODE 218
ISR_NO_ERROR_CODE 219
ISR_NO_ERROR_CODE 220
ISR_NO_ERROR_CODE 221
ISR_NO_ERROR_CODE 222
ISR_NO_ERROR_CODE 223
ISR_NO_ERROR_CODE 224
ISR_NO_ERROR_CODE 225
ISR_NO_ERROR_CODE 226
ISR_NO_ERROR_CODE 227
ISR_NO_ERROR_CODE 228
ISR_NO_ERROR_CODE 229
ISR_NO_ERROR_CODE 230
ISR_NO_ERROR_CODE 231
ISR_NO_ERROR_CODE 232
ISR_NO_ERROR_CODE 233
ISR_NO_ERROR_CODE 234
ISR_NO_ERROR_CODE 235
ISR_NO_ERROR_CODE 236
ISR_NO_ERROR_CODE 237
ISR_NO_ERROR_CODE 238
ISR_NO_ERROR_CODE 239
ISR_NO_ERROR_CODE 240
ISR_NO_ERROR_CODE 241
ISR_NO_ERROR_CODE 242
ISR_NO_ERROR_CODE 243
ISR_NO_ERROR_CODE 244
ISR_NO_ERROR_CODE 245
ISR_NO_ERROR_CODE 246
ISR_NO_ERROR_CODE 247
ISR_NO_ERROR_CODE 248
ISR_NO_ERROR_CODE 249
ISR_NO_ERROR_CODE 250
ISR_NO_ERROR_CODE 251
ISR_NO_ERROR_CODE 252
ISR_NO_ERROR_CODE 253
ISR_NO_ERROR_CODE 254
ISR_NO_ERROR_CODE 255

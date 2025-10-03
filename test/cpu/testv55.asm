
; Launch using:
; - x86emu -c v55 test/cpu/testv55.img

%macro	brkcs	1
%ifidn	%1, ax
	db	0x0F, 0x2D, 0xC0
%elifidn	%1, cx
	db	0x0F, 0x2D, 0xC1
%elifidn	%1, dx
	db	0x0F, 0x2D, 0xC2
%elifidn	%1, bx
	db	0x0F, 0x2D, 0xC3
%elifidn	%1, sp
	db	0x0F, 0x2D, 0xC4
%elifidn	%1, bp
	db	0x0F, 0x2D, 0xC5
%elifidn	%1, si
	db	0x0F, 0x2D, 0xC6
%elifidn	%1, di
	db	0x0F, 0x2D, 0xC7
%else
%error Unknown register %1
%endif
%endmacro

%macro	ds3 0
	db	0xD6
%endmacro

%macro	ds2 0
	db	0x63
%endmacro

%macro	iram 0
	db	0xF1
%endmacro

	org	0x7C00

	jmp	0:start
start:
	xor	ax, ax
	cli
	mov	ss, ax
	mov	sp, 0x7C00
	sti
	mov	ds, ax

	mov	ax, 0x0B80
	;mov ds3, ax
	db	0x8E, 0xF0
	mov	si, message1
	xor	di, di
	mov	ah, 0x1E
	mov	cx, message1_length

.1:
	lodsb
	ds3
	stosw
	loop	.1

	; vector PC
	iram
	mov	word [1 * 32 + 0x02], when_bank1
	; DS
	iram
	mov	word [1 * 32 + 0x08], ds
	; SS
	iram
	mov	word [1 * 32 + 0x0A], ss
	; CS
	iram
	mov	word [1 * 32 + 0x0C], cs
	; ES
	iram
	mov	word [1 * 32 + 0x0E], 0xB800
	; DI
	iram
	mov	word [1 * 32 + 0x10], 160
	; SI
	iram
	mov	word [1 * 32 + 0x12], message2
	; SP
	iram
	mov	word [1 * 32 + 0x16], sp
	; CX
	iram
	mov	word [1 * 32 + 0x1C], message2_length
	; AH
	iram
	mov	word [1 * 32 + 0x1F], 0x1E
	mov	ax, 1
	brkcs	ax

when_bank1:

.2:
	lodsb
	stosw
	loop	.2

.0:
	hlt
	jmp	.0

message1:
	db	"Using extended segment registers"
message1_length	equ	$ - message1

message2:
	db	"Using register bank 1"
message2_length	equ	$ - message2

	times	512-2-($-$$) db 0
	db	0x55, 0xAA




; Launch using:
; - x86emu -c v20 test/cpu/testv20.img

%include 'i8080.inc'

%macro	brkem	1
	db	0x0F, 0xFF, %1
%endmacro

%macro	calln	1
	db	0xED, 0xED, %1
%endmacro

%macro	retem	1
	db	0xED, 0xFD, %1
%endmacro

%define LOAD_ES 0x80
%define STORE_FAR 0x81
%define LOAD_FAR 0x82
%define OUT_FAR 0x83
%define IN_FAR 0x84
%define SYSTEM 0x8F

	org	0x7c00

	jmp	0:start
start:

	cli

	xor	ax, ax
	mov	ds, ax

	;;;; Native (8086) mode stack should be separate from 8080 emulation mode stack
	mov	ax, 0x1000
	mov	ss, ax
	xor	sp, sp

	;;;; Set up interrupt vectors to handle far address from 8080 emulation mode
	mov	word [4 * LOAD_ES], load_es
	mov	word [4 * LOAD_ES + 2], cs

	mov	word [4 * STORE_FAR], store_far
	mov	word [4 * STORE_FAR + 2], cs

	mov	word [4 * LOAD_FAR], load_far
	mov	word [4 * LOAD_FAR + 2], cs

	mov	word [4 * OUT_FAR], out_far
	mov	word [4 * OUT_FAR + 2], cs

	mov	word [4 * IN_FAR], in_far
	mov	word [4 * IN_FAR + 2], cs

	mov	word [4 * SYSTEM], start8080
	mov	word [4 * SYSTEM + 2], cs

	brkem	SYSTEM

.0:
	hlt
	jmp	.0

load_es:
	mov	es, bx
	iret

store_far:
	mov	[es:bx], al
	iret

load_far:
	mov	al, [es:bx]
	iret

out_far:
	out	dx, al
	iret

in_far:
	in	al, dx
	iret

	code8080

start8080:
	;;;; Set up stack pointer
	lxi	sp, 0x7c00

	;;;; Load VGA screen buffer into 8086 mode ES for accessing via far calls
	lxi	h, 0xb800

	calln	LOAD_ES

	;;;; Clear VGA screen with blue background and yellow foreground
	lxi	h, 0
	lxi	b, 80 * 25

.2:
	mvi	a, 0x20
	calln	STORE_FAR
	inx	h
	mvi	a, 0x1e
	calln	STORE_FAR
	inx	h
	dcx	b
	mov	a, b
	ora	c
	jnz	.2

	;;;; Display message
	lxi	d, message
	lxi	h, 0
	mvi	c, message_length

.1:
	ldax	d
	inx	d
	calln	STORE_FAR
	inx	h
	mvi	a, 0x1e
	calln	STORE_FAR
	inx	h
	dcr	c
	jnz	.1

	;;;; Move cursor to end of screen
	lxi	d, 0x03d4
	mvi	a, 0x0f
	calln	OUT_FAR
	inx	d
	mvi	a, (80 * 24 + 79) & 0xff
	calln	OUT_FAR
	lxi	d, 0x03d4
	mvi	a, 0x0e
	calln	OUT_FAR
	inx	d
	mvi	a, (80 * 24 + 79) >> 8
	calln	OUT_FAR

.0:
	hlt
	jmp	.0

message:
	db	"This code is executed from 8080 emulation mode"
message_length	equ	$ - message

	times	512-2-($-$$) db 0
	db	0x55, 0xAA


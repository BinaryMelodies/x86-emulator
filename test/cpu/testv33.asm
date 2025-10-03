
; Launch using:
; - x86emu -c v33 test/cpu/testv33.img

%macro	brkxa	1
	db	0x0F, 0xE0, %1
%endmacro

%macro	retxa	1
	db	0x0F, 0xF0, %1
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

	mov	ax, 0xB800
	mov	es, ax
	mov	si, message1
	xor	di, di
	mov	ah, 0x1E
	mov	cx, message1_length

.1:
	lodsb
	stosw
	loop	.1

;;;;
	mov	dx, 0xFF00
	xor	ax, ax
	mov	cx, 64
.2:
	out	(dx), ax
	add	dx, 2
	inc	ax
	loop	.2

	;;; page 0x8000 should map to 0xB8000
	mov	dx, 0xFF00 + ((0x8000 >> 13) & ~1)
	mov	ax, 0xB8000 >> 14
	out	(dx), ax

	;;; page 0xB8000 will map to 0x8000, to make sure we don't access it at the "wrong" address
	mov	dx, 0xFF00 + ((0xB8000 >> 13) & ~1)
	mov	ax, 0x8000 >> 14
	out	(dx), ax

	xor	ax, ax
	mov	es, ax
	mov	word [es:0x80 * 4], xa_start
	mov	[es:0x80 * 4 + 2], cs

	brkxa	0x80

xa_start:
	;mov	ax, 0xB800
	;mov	es, ax
	mov	si, message2
	mov	di, 0x8000 + 160
	mov	ah, 0x1E
	mov	cx, message2_length

.3:
	lodsb
	stosw
	loop	.3

	mov	word [es:0x80 * 4], rm_start
	mov	[es:0x80 * 4 + 2], cs
	retxa	0x80

rm_start:
	mov	ax, 0xB800
	mov	es, ax
	mov	si, message1
	mov	di, 160*2
	mov	ah, 0x1E
	mov	cx, message1_length

.4:
	lodsb
	stosw
	loop	.4

.0:
	hlt
	jmp	.0

message1:
	db	"Running in normal addressing mode"
message1_length	equ	$ - message1

message2:
	db	"Running in expanded addressing mode"
message2_length	equ	$ - message2

	times	512-2-($-$$) db 0
	db	0x55, 0xAA


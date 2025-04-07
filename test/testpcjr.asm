
; Launch using:
; - x86emu -P pcjr test/testpcjr.jrc

SEGMENT_VALUE	equ	0x1234

	org	-0x200

	db	"PCjr"
	times	0x1CE - ($ - $$)	db 0
	dw	SEGMENT_VALUE
	times	0x200 - ($ - $$)	db 0

	nop
	nop
	nop

start:
	mov	ax, cs
	call	putword
	mov	al, ':'
	call	putchar
	call	.1
.1:
	pop	ax
	add	ax, start - .1
	call	putword

.0:
	hlt
	jmp	.0

putnl:
	mov	byte [screen_x], 0
	inc	byte [screen_y]
	ret

putstr:
	lodsb
	test	al, al
	jz	.1
	call	putchar
	jmp	putstr
.1:
	ret

putword:
	push	ax
	mov	al, ah
	call	putbyte
	pop	ax

putbyte:
	push	ax
	mov	cl, 4
	shr	al, cl
	call	putnibble
	pop	ax

putnibble:
	and	al, 0x0F
	cmp	al, 10
	jc	.1
	add	al, 'A' - '0' - 10
.1:
	add	al, '0'

putchar:
	xchg	ax, di
	mov	al, [screen_y]
	xor	ah, ah
	mov	cx, 80
	mul	cx
	add	al, [screen_x]
	adc	ah, 0
	shl	ax, 1
	xchg	ax, di

	mov	ah, [screen_attribute]
	mov	cx, [screen_segment]
	mov	es, cx
	stosw

	mov	al, [screen_x]
	inc	al
	cmp	al, 80
	jc	.1
	xor	al, al
	inc	byte [screen_y]
.1:
	mov	[screen_x], al
	ret

exit:
	hlt
	jmp	exit

screen_segment:
	dw	0xB800

screen_x:
	db	0
screen_y:
	db	0
screen_attribute:
	dw	0x07



; Launch using:
; - x86emu -P mda  test/pc/testtext.com
; - x86emu -P cga  test/pc/testtext.com
; - x86emu -P pcjr test/pc/testtext.com

MACH_IBMPC_MDA	equ	1
MACH_IBMPC_CGA	equ	2
MACH_NECPC98	equ	3

	org	0x100

setup_screen:
	mov	ax, 0xFFFF
	mov	es, ax
	cmp	byte [es:0x0007], '/'
	jne	.necpc98
	cmp	byte [es:0x000A], '/'
	jne	.necpc98

	xor	ax, ax
	mov	es, ax
	cmp	byte [es:0x0449], 0x07
	jne	.ibmpccga

	mov	byte [machine_type], MACH_IBMPC_MDA
	mov	word [screen_segment], 0xB000
	jmp	start

.ibmpccga:
	mov	byte [machine_type], MACH_IBMPC_CGA
	mov	word [screen_segment], 0xB800
	jmp	start

.necpc98:
	mov	byte [machine_type], MACH_NECPC98
	mov	word [screen_segment], 0xA000
	mov	byte [screen_attribute], 0xE1

start:
	mov	al, [machine_type]
	call	putbyte
	mov	ax, 0x9A0F
	call	putword
	mov	al, 'H'
	call	putchar
	mov	al, 'i'
	call	putchar
	mov	si, text_greeting
	call	putstr

	mov	si, text_very_long_line
	call	putstr

	call	exit

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

	cmp	byte [machine_type], MACH_NECPC98
	je	.necpc

.ibmpc:
	mov	ah, [screen_attribute]
	mov	cx, [screen_segment]
	mov	es, cx
	stosw
	jmp	.1

.necpc:
	xor	ah, ah
	mov	cx, [screen_segment]
	mov	es, cx
	stosw
	add	di, 0x2000 - 2
	mov	al, [screen_attribute]
	stosw

.1:
	mov	al, [screen_x]
	inc	al
	cmp	al, 80
	jc	.2
	xor	al, al
	inc	byte [screen_y]
.2:
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
	db	0x07

text_greeting:
	db	"Greetings, Universe!", 0

text_very_long_line:
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	"12345678"
	db	0

machine_type:
	db	0


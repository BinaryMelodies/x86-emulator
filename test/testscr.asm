
; Launch using:
; - x86emu -P mda -O blink   test/testscr.com
; - x86emu -P mda -O noblink test/testscr.com
; - x86emu -P cga -O blink   test/testscr.com
; - x86emu -P cga -O noblink test/testscr.com
; - x86emu -P pc98           test/testscr.com

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
	mov	byte [screen_attribute], 0x07
	jmp	start

.ibmpccga:
	mov	byte [machine_type], MACH_IBMPC_CGA
	mov	word [screen_segment], 0xB800
	mov	byte [screen_attribute], 0x07
	jmp	start

.necpc98:
	mov	byte [machine_type], MACH_NECPC98
	mov	word [screen_segment], 0xA000
	mov	byte [screen_attribute], 0xE1

start:
	xor	di, di
	xor	ax, ax
	mov	ah, [screen_attribute]

draw_char_table:
	call	placechar

	inc	al
	test	al, 0xF
	jz	.1
	inc	di
	inc	di
	jmp	draw_char_table
.1:
	add	di, 160 - 2 * 15
	test	al, al
	jnz	draw_char_table

	mov	di, 40 * 2
	mov	ax, '@'

draw_attr_table:
	call	placechar

	inc	ah
	test	ah, 0xF
	jz	.1
	inc	di
	inc	di
	jmp	draw_attr_table
.1:
	add	di, 160 - 2 * 15
	test	ah, ah
	jnz	draw_attr_table

	call	exit

placechar:
	push	di
	push	ax

	cmp	byte [machine_type], MACH_NECPC98
	je	.necpc

.ibmpc:
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
	pop	ax
	push	ax
	mov	al, ah
	xor	ah, ah
	stosw

.1:

	pop	ax
	pop	di
	ret

exit:
	hlt
	jmp	exit

screen_segment:
	dw	0

machine_type:
	db	0

screen_attribute:
	db	0



	org	0x100

start:
	mov	ax, 0x10
	;mov	ds2, ax
	db	0x8E, 0xF8
	;ds2:
	db	0x63
	mov	byte [0], 1
	mov	ax, 0x100
	mov	es, ax
	xor	cx, cx
	mov	cl, byte [es:0]

	mov	ax, 0x20
	;mov	ds3, ax
	db	0x8E, 0xF0
	;ds3:
	db	0xD6
	mov	byte [0], 2
	mov	ax, 0x200
	mov	es, ax
	xor	cx, cx
	mov	cl, byte [es:0]

	push	word 0x1234
	;pop	ds2
	db	0x0F, 0x77
	;push	ds2
	db	0x0F, 0x76
	pop	ax

	push	word 0x5678
	;pop	ds3
	db	0x0F, 0x7F
	;push	ds3
	db	0x0F, 0x7E
	pop	ax

	;iram:
	db	0xF1
	mov	word [0x1E], 0xABCD

	hlt


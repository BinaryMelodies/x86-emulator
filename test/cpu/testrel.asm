
	; Test IP-relative addressing

	bits	64

	mov	al, 0
	mov	byte [rel variable], 0x42
	mov	al, byte [rel variable]

.0:
	hlt
	jmp	.0

variable:
	db	0


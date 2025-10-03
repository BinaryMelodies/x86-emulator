
	org	0x100

.1:
	mov	ah, 6
	mov	dl, 0xFF
	int	0x21
	push	ax

	mov	ah, 6
	mov	dl, '!'
	int	0x21

	pop	ax
	test	al, al
	jz	.1

	int	0x20


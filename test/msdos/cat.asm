
	org	0x100

_start:
	mov	ah, 0x01
	int	0x21
	cmp	al, 'q'
	je	.exit
	mov	ah, 0x02
	mov	dl, al
	int	0x21
	jmp	_start
.exit:
	int	0x20


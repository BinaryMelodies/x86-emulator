
%include 'i8080.inc'

	code8080
	org	0x100

_start:
	mvi	c, 0x01
	call	0x0005
	cpi	'q'
	jz	.exit
	mvi	c, 0x02
	mov	e, a
	call	0x0005
	jmp	_start
.exit:
	rst	0


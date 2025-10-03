
%include 'i8080.inc'

	code8080
	org	0x100

.1:
	mvi	c, 6
	mvi	e, 0xFF
	call	5
	push	psw

	mvi	c, 6
	mvi	e, '!'
	call	5

	pop	psw
	ora	a
	jz	.1

	rst	0


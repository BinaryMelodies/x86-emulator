
%include 'i8080.inc'

	code8080
	org	0x100

	lxi	d, message
	mvi	c, 9
	call	5
	rst	0

message:
	db	"Hello!", 13, 10, '$'



	org	0x100

	mov	dx, message
	mov	ah, 9
	int	0x21

	int	0x20

message:
	db	"Hello!", 13, 10, '$'


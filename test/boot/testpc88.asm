
; Launch using:
; - x86emu -P pc88va test/boot/testpc88.img

; PC-88 VA boot

	; 3000:0000
	org	0

	push	cs
	pop	ds

	mov	ax, 0xA600
	mov	es, ax
	mov	si, message
	xor	di, di
	xor	ah, ah
	mov	cx, message_length

.1:
	lodsb
	stosw
	loop	.1

	mov	ax, 0xAE00
	mov	es, ax
	xor	di, di
	mov	ax, 0x07
	mov	cx, message_length
	rep	stosw

.0:
	hlt
	jmp	.0

message:
	db	"Greetings!"
message_length	equ	$ - message


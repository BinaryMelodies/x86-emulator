
; Launch using:
; - x86emu -P act test/boot/testact.img

; Apricot boot

	org	-0x200

	times	11	db	0
	db	1	; bootable
	times	2	db	0
	dw	0x200	; sector size
	times	10	db	0
	dd	1	; boot image start sector
	dw	1	; boot image sector count
	dw	0, 0x0BC0	; load address
	dw	0, 0x0BC0	; start address
	times	0x200 - ($ - $$)	db	0

	push	cs
	pop	ds

	mov	ax, 0xF000
	mov	es, ax
	xor	di, di
	mov	si, message
	mov	ah, 0
	mov	cx, message_length

.1:
	lodsb
	add	ax, 0x40
	stosw
	loop	.1

.0:
	hlt
	jmp	.0

message:
	db	"Greetings!"
message_length	equ	$ - message

	times	0x200 - (($ - $$) & 0x1FF)	db	0


; using the code in https://drdobbs.com/embedded-systems/processor-detection-schemes/184409011

	org	0x100

start:
	std

	mov	dx, 1
	mov	di, end_label
	mov	al, 0x90 ; NOP instruction
	mov	cx, 3
	rep	stosb

	cld
	nop
	nop
	nop
	dec	dx
	nop
end_label:
	nop

	test	dx, dx
	jz	.is_8088

.is_8086:
	mov	si, text_8086
	jmp	.print_message

.is_8088:
	mov	si, text_8088

.print_message:
	mov	ax, 0xB800
	mov	es, ax
	xor	di, di
	mov	ah, 0x07

.1:
	lodsb
	test	al, al
	jz	.2
	stosw
	jmp .1

.2:
end:
	hlt
	jmp	end

text_8086:
	db	"Intel 8086 or NEC V30", 0

text_8088:
	db	"Intel 8088 or NEC V20", 0


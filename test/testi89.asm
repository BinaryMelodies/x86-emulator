
; Launch using:
; - x86emu -m i89 test/testi89.bin

	org	1000h

	lpdi	ga, message
	lpdi	gb, 0B8000000h
	movi	bc, message_length
	movi	ix, 0
next:
	movb	cc, [ga]
	ori	cc, 01E00h
	mov	[gb+ix+], cc
	inc	ga
	dec	bc
	jnz	bc, next
	hlt

message:
	db	047h, 072h, 065h, 065h, 074h, 069h, 06eh, 067h, 073h, 021h
message_length	equ	10


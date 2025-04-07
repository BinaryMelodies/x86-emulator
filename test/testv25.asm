
; Launch using:
; - x86emu -c v25 test/testv25.img

%macro	brkcs	1
%ifidn	%1, ax
	db	0x0F, 0x2D, 0xC0
%elifidn	%1, cx
	db	0x0F, 0x2D, 0xC1
%elifidn	%1, dx
	db	0x0F, 0x2D, 0xC2
%elifidn	%1, bx
	db	0x0F, 0x2D, 0xC3
%elifidn	%1, sp
	db	0x0F, 0x2D, 0xC4
%elifidn	%1, bp
	db	0x0F, 0x2D, 0xC5
%elifidn	%1, si
	db	0x0F, 0x2D, 0xC6
%elifidn	%1, di
	db	0x0F, 0x2D, 0xC7
%else
%error Unknown register %1
%endif
%endmacro

	org	0x7C00

	jmp	0:start
start:
	xor	ax, ax
	cli
	mov	ss, ax
	mov	sp, 0x7C00
	sti
	mov	ds, ax

	mov	ax, 0xB800
	mov	es, ax
	xor	di, di
	mov	dh, 0x1E

	mov	ax, 0x1A2B
	call	put_word

	mov	ax, 0xFFE0
	mov	es, ax
	; vector PC
	mov	word [es:1 * 32 + 0x02], when_bank1
	; DS
	mov	word [es:1 * 32 + 0x08], ds
	; SS
	mov	word [es:1 * 32 + 0x0A], ss
	; CS
	mov	word [es:1 * 32 + 0x0C], cs
	; ES
	mov	word [es:1 * 32 + 0x0E], 0xB800
	; DI
	mov	word [es:1 * 32 + 0x10], di
	; SP
	mov	word [es:1 * 32 + 0x16], sp
	; DH
	mov	word [es:1 * 32 + 0x1B], 0x1E
	; AX
	mov	word [es:1 * 32 + 0x1E], 0xBABE ; this value should be printed out
	mov	ax, 1
	brkcs	ax

.0:
	hlt
	jmp	.0

when_bank1:
	call	put_word

	hlt
.0:
	jmp	.0

put_word:
	push	ax
	mov	al, ah
	call	put_byte
	pop	ax

put_byte:
	push	ax
	mov	cl, 4
	shr	al, cl
	call	put_nibble
	pop	ax

put_nibble:
	and	al, 0xF
	cmp	al, 10
	jc	.1
	add	al, 'A' - '0' - 10
.1:
	add	al, '0'

put_char:
	mov	ah, dh
	stosw
	ret


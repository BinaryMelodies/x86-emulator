
	; The boot sector should be loaded at linear address 0x00007C00, but we want our code to be based from 0x7C00
	org	0
	bits	16

start:
	; Memory offset that stores the current screen X position
%define SCREEN_X 512
	; Memory offset that stores the current screen Y position
%define SCREEN_Y 513
	; Memory offset to a 16-byte aligned area, up to 512 bytes might be overwritten by FXSAVE
%define BUFFER   528

	; Set CS=0x07C0, but otherwise continue execution as usual
	call	0x07C0:.1
.1:
	; Set SS=DS=0x07C0 as well for tiny memory model
	; Also, the top of the stack will be at the first byte of the executable
	mov	ax, 0x07C0
	mov	ds, ax
	mov	ss, ax
	xor	sp, sp

	; Initialize the cursor position to the top left
	xor	ax, ax
	mov	[SCREEN_X], ax

%if 0
	; Issue an x87 NOP instruction and store the FPU state in BUFFER (original 8087 instruction)
	fnop
	fnsave	[BUFFER]

	; Display the 20-bit real mode FIP
	mov	ax, [BUFFER + 8]
	shr	ax, 12
	call	putword
	mov	ax, [BUFFER + 6]
	call	putword
	call	putnl
%endif

%if 0
	; Store the FPU state in BUFFER (original 8087 instruction)
	fnsave	[BUFFER]

	; Modify the FDP pointer to 0x12345
	mov	word [BUFFER + 10], 0x2345
	and	byte [BUFFER + 13], ~0xF0
	or	byte [BUFFER + 13], 0x1 << 4

	; Restore the FPU state in BUFFER (original 8087 instruction)
	frstor	[BUFFER]
%endif

%if 0
	; Store the FPU state in BUFFER using FXSAVE (Pentium III/SSE and later)
;	fnop
	fxsave	[BUFFER]

	; Display the 64-bit area containing FDS and FDP
	mov	ax, [BUFFER + 22]
	call	putword
	mov	ax, [BUFFER + 20]
	call	putword
	mov	ax, [BUFFER + 18]
	call	putword
	mov	ax, [BUFFER + 16]
	call	putword
%endif

	; Set up temporary protected mode
	lgdt	[gdtr]
	mov	eax, cr0
	or	al, 1
	mov	cr0, eax

	; Load protected mode data selector
	mov	ax, 0x08
	mov	es, ax

	; Shut down protected mode, with ES still containing a protected mode selector
	mov	eax, cr0
	and	al, ~1
	mov	cr0, eax

	; Issue an x87 memory instruction and store the FPU state in BUFFER (original 8087 instruction)
	fld	dword [es:dword 0x12345]
	o32 fnsave	[BUFFER]

	; Display the 32-bit real mode FDP
	mov	eax, [BUFFER + 24]
	shr	eax, 12
	call	putword
	mov	ax, [BUFFER + 20]
	call	putword
	call	putnl

	; Halt in an infinite loop
.0:
	hlt
	jmp	.0

	; Change cursor position to start of following line (note: no checks are made for bottom line)
putnl:
	mov	byte [SCREEN_X], 0
	inc	byte [SCREEN_Y]
	ret

	; Display a 16-bit value in AX as a hexadecimal string
putword:
	push	ax
	mov	al, ah
	call	putbyte
	pop	ax
	;jmp	putbyte

	; Display an 8-bit value in AL as a hexadecimal string
putbyte:
	push	ax
	shr	al, 4
	call	putnibble
	pop	ax
	;jmp	putnibble

	; Display a single 4-bit hexadecimal digit in AL
putnibble:
	and	al, 0xF
	cmp	al, 10
	jc	.1
	add	al, 'A' - 10 - '0'
.1:
	add	al, '0'
	;jmp	putchar

	; Display an ASCII character on the VGA text buffer, move cursor
putchar:
	mov	bx, 0xB800
	mov	es, bx
	mov	bl, [SCREEN_Y]
	xor	bh, bh
	imul	bx, bx, 80
	add	bl, [SCREEN_X]
	adc	bh, 0
	shl	bx, 1
	mov	di, bx
	mov	ah, 0x1E
	stosw
	mov	bl, [SCREEN_X]
	inc	bl
	cmp	bl, 80
	jc	.1
	xor	bl, bl
	inc	byte [SCREEN_Y]
.1:
	mov	[SCREEN_X], bl
	ret

gdtr:
	dw	gdt_end - gdt_start
	dd	0x7C00 + gdt_start
gdt_start:
	dw	0, 0, 0, 0
	dw	0xFFFF, 0x3000, 0x9202, 0x008F ; data segment with full 4GiB limit and 0x00023000 starting address
gdt_end:

	; Mark the first sector as bootable
	times	512 - ($ - $$) - 2 db 0
	db	0x55, 0xAA



; Launch using:
; - x86emu test/86.img
; - x86emu test/286.img
; - x86emu test/386.img
; - x86emu test/x64.img

%macro	_descriptor	3
	dw	(%2) & 0xFFFF, (%1) & 0xFFFF, (((%1) >> 16) & 0x00FF) | (((%3) & 0x00FF) << 8), (((%2) >> 16) & 0x000F) | (((%3) & 0xF000) >> 8) | (((%1) >> 16) & 0xFF00)
%endmacro

%macro	descriptor	3
%if	((%3) & 0x8000) || (%2) > 0xFFFF
	_descriptor	%1, (%2) >> 12, (%3) | 0x8000
%else
	_descriptor	%1, %2, %3
%endif
%endmacro

	org	0x7c00

	bits	16

image_start:
	; Clear CS
	jmp	0:rm_start
rm_start:
	; Set up stack
	xor	ax, ax
	mov	ss, ax
	mov	sp, 0x7c00
	; Make DS=CS
	mov	ds, ax

;	; Load rest of image
;	;xor	ax, ax
;	mov	es, ax
;	mov	bx, 0x7c00
;	mov	ax, 0x0200 | ((image_end - image_start + 0x1FF) >> 9)
;	mov	cx, 0x0001 ; cylinder[0:7]:cylinder[8:9]:sector[0:5]
;	mov	dx, 0x0000 ; head:drive (floppy, harddisk: 0x80)
;	int	0x13

;	; Clear A20 line (fast)
;	in	al, 0x92
;	or	al, 2
;	out	0x92, al

; test
;	mov	ax, 0xb800
;	mov	es, ax
;	xor	di, di
;	mov	ax, 0x4E40
;	stosw
;.0:
;	jmp	.0

%ifdef OS286
	cli
	lgdt	[gdtr]
	; Enter protected mode (in a 286 compatible way)
	smsw	ax
	or	al, 1
	lmsw	ax
	; Load CS
	jmp	0x08:pm_start
%elifdef OS386
	cli
	lgdt	[gdtr]
	; Enter protected mode
	mov	eax, cr0
	or	al, 1
	mov	cr0, eax
	; Load CS
	jmp	0x08:pm_start
%elifdef OSX64
	; Set up identity paging tables
	mov	di, 0x1000
	push	di
	mov	ecx, 0x1000
	xor	eax, eax
	mov	es, ax
	cld
	rep stosd
	pop	di

	push	di
	mov	cx, 3
.setup_directories:
	lea	eax, [di + 0x1000]
	or	ax, 3
	mov	[di], eax
	add	di, 0x1000
	loop	.setup_directories

	mov	eax, 3
	mov	ecx, 0x200
.setup_id_paging:
	mov	[di], eax
	add	eax, 0x1000
	add	di, 8
	loop	.setup_id_paging

	xor	edi, edi
	pop	di

	cli
	lgdt	[gdtr]
	; Enable paging extensions and PAE
	mov	eax, 0x000000a0
	mov	cr4, eax
	; Load page tables
	mov	cr3, edi
	; Enable long mode
	mov	ecx, 0xC0000080
	rdmsr
	or	ax, 0x0100
	wrmsr
	; Enter protected mode and enable paging
	mov	eax, cr0
	or	eax, 0x80000001
	mov	cr0, eax
	; Load CS
	jmp	0x08:pm_start
%endif

%ifdef OS86
	; Write message
	mov	ax, 0xb800
	mov	es, ax
	xor	di, di
	mov	si, message
	mov	cx, message_length
	mov	ah, 0x1E
%elifdef OS286
pm_start:
	; Set up stack and other segment registers
	mov	sp, 0x7c00
	mov	ax, 0x10
	mov	ss, ax
	mov	ds, ax
	mov	es, ax

	; Write message
	mov	ax, 0x18
	mov	es, ax
	xor	di, di
	mov	si, message
	mov	cx, message_length
	mov	ah, 0x1E
%elifdef OS386
	bits	32
pm_start:
	; Set up stack and other segment registers
	mov	esp, 0x7c00
	mov	ax, 0x10
	mov	ss, ax
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax

	; Write message
	mov	edi, 0xb8000
	mov	esi, message
	mov	ecx, message_length
	mov	ah, 0x1E
%elifdef OSX64
	bits	64
pm_start:
	; Set up stack and other segment registers
	mov	esp, 0x7c00
	mov	ax, 0x10
	mov	ss, ax
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax

	; Write message
	mov	edi, 0xb8000
	mov	esi, message
	mov	ecx, message_length
	mov	ah, 0x1E
%endif

.1:
	lodsb
	stosw
	loop	.1

	; Halt
	cli
.2:
	hlt
	jmp	.2

%ifdef OS286
	align	4, db 0
gdtr:
	dw	gdt_end - gdt - 1
	dd	gdt

	align	8, db 0
gdt:
	dw	0, 0, 0, 0
	descriptor	0, 0xFFFF, 0x009A
	descriptor	0, 0xFFFF, 0x0092
	descriptor	0xB8000, 0xFFFF, 0x0092
gdt_end:
%elifdef OS386
	align	4, db 0
gdtr:
	dw	gdt_end - gdt - 1
	dd	gdt

	align	8, db 0
gdt:
	dw	0, 0, 0, 0
	descriptor	0, 0xFFFFFFFF, 0x409A
	descriptor	0, 0xFFFFFFFF, 0x4092
gdt_end:
%elifdef OSX64
	align	4, db 0
gdtr:
	dw	gdt_end - gdt - 1
	dd	gdt

	align	8, db 0
gdt:
	dw	0, 0, 0, 0
	descriptor	0, 0, 0x209A
	descriptor	0, 0, 0x0092
gdt_end:
%endif

message:
	db	"Greetings! Executing in "
%ifdef OS86
	db	"real mode"
%elifdef OS286
	db	"16-bit protected mode"
%elifdef OS386
	db	"32-bit protected mode"
%elifdef OSX64
	db	"64-bit long mode"
%else
	db	"some unknown mode"
%endif
	db	"!"
message_length	equ	$ - message


	times	510 - ($ - $$)	db 0

	; Needed for BIOS to boot disk
	db	0x55, 0xAA

	; Whole number of 512-byte sectors
	align	512, db 0
image_end:


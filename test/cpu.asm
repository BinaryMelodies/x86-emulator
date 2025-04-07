
; Launch using:
; - x86emu -c <CPU architecture name> test/cpu.com

	org	0x100

	bits	16

start:

.test_80286:
	pushf
	pop	ax
	mov	cx, ax
	and	ax, 0x0FFF
	push	ax
	popf
	pushf
	pop	ax
	and	ax, 0xF000
	cmp	ax, 0xF000
	jne	.test_80386

.is_808x_or_8018x:
	cli
	mov	bx, [0xFFFF]
	mov	dx, bx
	add	dx, 0x0101
	mov	[0xFFFF], dx
	cmp	[0], dh
	mov	[0xFFFF], bx
	sti
	jne	.is_8018x_or_nec

.is_808x_or_nec:
	mov	al, 10
	aam	8
	cmp	al, 2
	jne	.is_nec

	mov	si, text_8086
	jmp	.print

.is_nec:
	mov	si, text_nec
	jmp	.print

.is_8018x_or_nec:
	xor	ax, ax
	push	ax
	popf
	pushf
	pop	ax
	test	al, 2
	jnz	.is_80186

.is_nec_v25_v55:
	mov	si, text_nec_v25
	jmp	.print

.is_80186:
	mov	si, text_80186
	jmp	.print

.test_80386:
	or	cx, 0xF000
	push	cx
	popf
	pushf
	pop	ax
	and	ax, 0xF000
	jnz	.test_80486

.is_80286:
	mov	si, text_80286
	jmp	.print

.test_80486:
	pushfd
	pop	eax
	mov	ecx, eax
	xor	eax, 0x40000
	push	eax
	popfd
	pushfd
	pop	eax
	xor	eax, ecx
	jnz	.test_80586
	push	ecx
	popfd

.is_80386:
	mov	si, text_80386
	jmp	.print

.test_80586:
	mov	eax, ecx
	xor	eax, 0x200000
	push	eax
	popfd
	pushfd
	pop	eax
	xor	eax, ecx
	jnz	.test_vendor

.is_80486:
	mov	si, text_80486
	jmp	.print

.test_vendor:
	mov	eax, 0
	cpuid
	mov	[vendor_id], ebx
	mov	[vendor_id + 4], edx
	mov	[vendor_id + 8], ecx

;	mov	eax, 0
;.scan_vendor_id:
;	mov	cx, 12
;	mov	si, vendor_id
;	mov	di, [id_list + 4 * eax]
;	repe	scasd
;	jz	.test_family
;	inc	ax
;	cmp	ax, 5
;	jb	.scan_vendor_id

;	mov	si, text_80586
;	jmp	.print

.test_family:
;	mov	si, [vendor_name_list + 4 * eax]

	mov	eax, 1
	cpuid
	shr	eax, 8
	and	al, 0xF
	cmp	al, 10
	jc	.3
	add	al, 'A' - (10 + '0')
.3:
	add	al, '0'

	mov	dx, 0xB800
	mov	es, dx
	mov	di, 160
	mov	ah, 0x07
	mov	[es:di], ax

	mov	si, vendor_id
	jmp	.print

.print:
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
	db	"Intel 8086/8088", 0

text_nec:
	db	"NEC V20/V30 or later NEC", 0

text_nec_v25:
	db	"NEC V25/V35 or NEC V55", 0

text_80186:
	db	"Intel 80186/80188", 0

text_80286:
	db	"Intel 80286", 0

text_80386:
	db	"Intel 80386", 0

text_80486:
	db	"Intel 80486", 0

text_80586:
	db	"Intel Pentium or later", 0

;id_list:
;	dw	intel_id, amd_id, cyrix_id, via_id, nsc_id, zhaoxin_id

;intel_id:
;	db	"GenuineIntel"
;amd_id:
;	db	"AuthenticAMD"
;cyrix_id:
;	db	"CyrixInstead"
;nsc_id:
;	db	"Geode by NSC"
;via_id:
;	db	"VIA VIA VIA "
;zhaoxin_id:
;	db	"  Shanghai  "

;vendor_name_list:
;	dw	intel_name, amd_name, cyrix_name, via_name, nsc_name, via_name, zhaoxin_name
;intel_name:
;	db	"Intel", 0
;amd_name:
;	db	"AMD", 0
;cyrix_name:
;	db	"Cyrix", 0
;nsc_name:
;	db	"National Semiconductors", 0
;via_name:
;	db	"VIA", 0
;zhaoxin_name:
;	db	"Zhaoxin", 0

vendor_id:
	times	13	db	0


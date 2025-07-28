
; Launch using:
; - x86emu -c <CPU architecture name> test/cpu.com

; based on https://www.rcollins.org/ddj/Sep96/Sep96.html

	org	0x100

	bits	16

start:
	jmp	.test_80286

; using the code in https://drdobbs.com/embedded-systems/processor-detection-schemes/184409011

	; Tests the instruction prefetch queue size
	; For the Intel 8088/80188/V20, the prefetch queue is 4 bytes
	; For the Intel 8086/80186/V30, the prefetch queue is 6 bytes
	; To check the queue, we execute a string instruction that overwrites the instruction stream appearing after the string instruction
	; If the prefetch queue is long enough, this will not impact the execution flow
	; If the prefetch quque is not long enough, the newly written instruction will be executed
	; In this implementation, a NOP instruction is used to overwrite the DEC DX instruction, permitting checking the DX register value
	; afterwards to distinguish between CPUs with shorter and longer prefetch queues
.test_cache:
	std

	push	ds
	pop	es
	mov	dx, 1
	mov	di, .end_label
	mov	al, 0x90 ; NOP instruction
	mov	cx, 3
	rep	stosb

	cld
	nop
	nop
	nop
	dec	dx
	nop
.end_label:
	nop

	test	dx, dx
	jnz	.is_4_byte

.is_6_byte:
	mov	al, 2
	ret

.is_4_byte:
	mov	al, 1
	ret

	; On the 8086/8088/80186/80188 and NEC V33, the highest 4 bits of the FLAGS register are hardwired to 1
	; On the NEC V20/V30, the highest bit (mode bit) cannot be changed outside of an interrupt (occuring from 8080 emulation mode)
	; On the NEC V25 and V55, the highest 4 bits (register bank, and a 1 bit or mode bit for the V25) cannot be changed by a PUSHF instruction
	; So in this test we attempt to clear the highest 4 bits of FLAGS using POPF/PUSHF and if they are all 1, it is one of the above mentioned CPUs
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

	; We need to check the pre-80186 CPUs first, since those do not have long range conditional jumps
	; On the 8086/8088 and V20/V30, writing a word to offset 0xFFFF within a segment overflows and accesses the byte at 0x10000 from the base
	; On a 80186/80188 and V25/V55, the write overflows to the byte at offset 0 of the segment
	; By writing a word at offset 0xFFFF and checking if the byte at offset 0 has changed, we can distinguish between these CPUs
	; (Note that on a 80286 or higher, writing at word offset 0xFFFF will result in a fault, since it crosses the segment limit)
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

	; On Intel CPUs, the AAD instruction can be used for arbitrary number bases, but on NEC CPUs, the base defaults to 10
.is_808x_or_nec:
	mov	ax, 0x0100
	aad	8
	cmp	al, 8
	jne	.is_nec

	; Check the prefetch queue size to distinguish between the 8088 and 8086
.is_808x:
	call	.test_cache
	cmp	al, 2
	je	.is_8086

.is_8088:
	mov	si, text_8088
	jmp	.print

.is_8086:
	mov	si, text_8086
	jmp	.print

	; Check the prefetch queue size to distinguish between the V20 and V30
.is_nec:
	call	.test_cache
	cmp	al, 2
	je	.is_v30

.is_v20:
	mov	si, text_v20
	jmp	.print

.is_v30:
	mov	si, text_v30
	jmp	.print

	; On all Intel CPUs, bit 1 of the FLAGS register is hardwired to be 1
	; The NEC V25 and V55 redefine bit 1 to be the complement of the trap-on-I/O-instruction bit
	; We can use this to distinguish between Intel and NEC CPUs
.is_8018x_or_nec:
	xor	ax, ax
	push	ax
	popf
	pushf
	pop	ax
	test	al, 2
	jnz	.is_8018x

	; Like all Intel CPUs, the V55 CPU hardwires bits 3 and 5 of the FLAGS register to 0
	; On the V25, these bits can be altered arbitrarily by code
.is_nec_v25_v55:
	or	al, 0x28
	push	ax
	popf
	pushf
	pop	ax
	test	al, 0x28
	jnz	.is_v25

.is_v55:
	mov	si, text_v55
	jmp	.print

.is_v25:
	mov	si, text_v25
	jmp	.print

	; Check the prefetch queue size to distinguish between the 80188 and 80186
.is_8018x:
	call	.test_cache
	cmp	al, 2
	je	.is_80186

.is_80188:
	mov	si, text_80188
	jmp	.print

.is_80186:
	mov	si, text_80186
	jmp	.print

	; On the 80286, the highest 4 bits of the FLAGS register (IOPL, NT and hardwired 1) cannot be changed to 1 in real mode
	; The 80386 and later, all of these but the highest bit can be set
	; By attempting to set them all, we can distinguish between the 80286 and later CPUs
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

	; If this is a 80386 or later, we have access to 32-bit registers, including EFLAGS
	; Bit 18 of the EFLAGS register is the AC flag, introduced on the 80486, hardwired to 0 on CPUs where it is not supported
	; By attempting to flip it, we can distinguish between the 80386 and later CPUs
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

	; Bit 21 of the EFLAGS register is the ID flag, first introduced on the Intel Pentium
	; Its presence permits using the CPUID instruction for finer differentiation between CPUs
	; Again, we will attempt to flip this bit and check if it changes
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

	; If the CPUID instruction exists, we can check the vendor ID and family
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
	; Convert family value into an ASCII character
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
	db	"Intel 8086", 0

text_8088:
	db	"Intel 8088", 0

text_v30:
	db	"NEC V30", 0

text_v20:
	db	"NEC V20", 0

text_v25:
	db	"NEC V25/V35", 0

text_v55:
	db	"NEC V55", 0

text_80186:
	db	"Intel 80186", 0

text_80188:
	db	"Intel 80188", 0

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


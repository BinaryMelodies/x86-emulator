
; Launch using (debugging on to see results):
; - x86emu -c 8086 -f 8087 -d test/testx87.com

	org	0x100

	fninit
	fld	tword [input]
	fld1
	fadd
	fstp	tword [output]

	hlt

input:
	dt	1.23

output:
	dt	0.0


#! /usr/bin/python3

import json
import zipfile
import gzip

MEMBLOCK = []
def print_test(entry, info, file):
	global MEMBLOCK
	#print(entry, file = sys.stderr)

	# Input registers
	iregs = []
	for reg, value in entry['initial']['regs'].items():
		iregs.append(f".{reg} = 0x{value:04X}")

	# Initial memory assignments
	imem = len(MEMBLOCK)
	for addr, value in entry['initial']['ram']:
		MEMBLOCK.append(f"{{ .address = 0x{addr:05X}, .value = 0x{value:04X} }}")
	MEMBLOCK.append("{ .terminate = true }")

	# Final memory assignments
	omem = len(MEMBLOCK)
	for addr, value in entry['final']['ram']:
		MEMBLOCK.append(f"{{ .address = 0x{addr:05X}, .value = 0x{value:04X} }}")
	MEMBLOCK.append("{ .terminate = true }")

	# Flags mask, avoid checking undocumented flags
	flags_mask = info.get('flags-mask', 0xFFFF)

	# Output registers
	oregs = []
	for reg, value in entry['final']['regs'].items():
		oregs.append(f".{reg} = 0x{value:04X}")
	for reg, value in entry['initial']['regs'].items():
		if reg not in entry['final']['regs']:
			oregs.append(f".{reg} = 0x{value:04X}")

	print(f"{{ .iregs = {{ {', '.join(iregs)} }}, .oregs = {{ {', '.join(oregs)} }}, .flags_mask = 0x{flags_mask:04X}, .imem = &testcase_memory_changes[{imem}], .omem = &testcase_memory_changes[{omem}] }},", file = file)

ZIPPATH = '../external/8088-main.zip'
INTPATH = '8088-main/v2/'

zfp = zipfile.ZipFile(ZIPPATH, 'r')
opcodes = json.load(zfp.open(INTPATH + 'metadata.json', 'r'))['opcodes']

for name in zfp.namelist():
	if name == INTPATH:
		continue
	if not name.startswith(INTPATH) or not name.endswith('.json.gz'):
		continue

	MEMBLOCK.clear()
	zfp2 = gzip.open(zfp.open(name, 'r'), 'r')
	print(name)
	obj = json.load(zfp2)
	testname = name[name.rfind('/') + 1:-8]
	if '.' in testname:
		key, reg = testname.split('.')
		info = opcodes[key]['reg'][reg]
	else:
		info = opcodes[testname]
	with open('8088/' + testname + '.gen.c', 'w') as file:
		print("struct mem testcase_memory_changes[];", file = file)
		print("struct testcase testcases[] =\n{", file = file)
		for entry in obj:
			print_test(entry, info, file = file)
		print("};", file = file)
		print("struct mem testcase_memory_changes[] = { ", end = '', file = file)
		print(", ".join(MEMBLOCK), end = '', file = file)
		print(" };", file = file)


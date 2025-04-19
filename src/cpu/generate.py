#! /usr/bin/python3

import os
import sys
import yaml

def recursive_replace(entity, text, newtext):
	""" Replace substrings, including as part of tuples, lists and dictionaries """
	if type(entity) is str:
		return entity.replace(text, newtext)
	elif type(entity) is tuple:
		return tuple(recursive_replace(subentity, text, newtext) for subentity in entity)
	elif type(entity) is list:
		return [recursive_replace(subentity, text, newtext) for subentity in entity]
	elif type(entity) is dict:
		return {key: recursive_replace(subentity, text, newtext) for key, subentity in entity.items()}
	else:
		assert False

class Path:
	"""
		Represents an instruction opcode encoded as a sequence of steps in the parsing process
		Steps would include a byte to select the opcode, checking the MOD, REG, MEM fields in a ModRM byte, and inspecting a 0x66/0xF3/0xF2 SIMD prefix
		Each step is represented as an integer/string pair where string is the type of inspection used
	"""
	def __init__(self, *index):
		assert all(type(item[0]) is int and type(item[1]) is str for item in index)
		modfield = False
		i = 0
		while i < len(index):
			if index[i][1] == 'modfield':
				if modfield:
					index = index[:i] + index[i + 1:]
					continue
				modfield = True
			i += 1
		self.items = list(index)

	def __repr__(self):
		return "Path(" + ', '.join(map(repr, self.items)) + ")"

	def __add__(self, other):
		return Path(*self.items + other.items)

	def __getitem__(self, index):
		if type(index) is slice:
			return Path(*self.items[index])
		else:
			return self.items[index]

	def __len__(self):
		return len(self.items)

	def __iter__(self):
		return iter(self.items)

	def __str__(self):
		""" Generates a representation that would appear in Intel documents """
		parts = []
		for value, selector in self.items:
			if selector == 'subtable':
				parts.append(f'{value:02X}')
			elif selector == 'modfield':
				parts.append(['M', 'R'][value])
			elif selector == 'regfield':
				parts.append(f'/{value}')
			elif selector == 'memfield':
				parts.append(f'+{value}')
			elif selector == 'prefix':
				if value != 0:
					parts.insert(0, [None, '66', 'F3', 'F2'][value])
			else:
				print(selector, value, file = sys.stderr)
				assert False
		return ' '.join(parts)

	def __eq__(self, other):
		return isinstance(other, Path) and self.items == other.items

	def separate_modfield(self, modfield):
		""" Creates a new path with the MOD field entry set to modfield """
		for index, (value, selector) in enumerate(self.items):
			if selector == 'modfield':
				return self
			elif selector in {'regfield', 'memfield'}:
				return self[:index] + Path((modfield, 'modfield')) + self[index:]
			elif selector == 'prefix':
				# final element, insert before
				return self[:index] + Path((modfield, 'modfield')) + self[index:]
		return self + Path((modfield, 'modfield'))

# Instructions and tables are stored as a dictionary of properties, specified by "keywords"
# Typical keywords for instructions:
# - mnem: The mnemonic used for the instruction
# - opds: The operands of the instruction
# - nec/intel: Alternative syntaxes, stored as a (mnem, opds) pair
# Typical keywords for tables:
# - discriminator: Which field in the opcode byte sequence determines the selection for the table, possible values: subtable, modfield, regfield, memfield, prefix
# Typical keywords for both:
# - modrm: Whether the instruction takes a ModRM byte or the table requires a ModRM byte to descend further
# - isas: List of CPU architectures implementing this instruction (pre-CPUID)
# - features: Which CPUID features control this, may contain a single string and multiple ('!', string) pairs
# - flags: Set of boolean flags
# -- fallback: only use if no definition is given for SIMD prefix

class Instruction:
	def __init__(self, **kwds):
		self.kwds = kwds

	def __repr__(self):
		return "Instruction(" + ", ".join(f"{key}={value!r}" for key, value in self.kwds.items()) + ")"

	def deepcopy(self):
		return Instruction(**self.kwds)

	@staticmethod
	def is_mem_opd(opd):
		""" Whether the operand is a ModRM memory operand """
		return opd in {'M', 'Mb', 'Mw', 'Md', 'Mz', 'Mv', 'Ma', 'Mp', 'M10', 'Mq/Mo', 'M14/M28', 'M94/M108', 'mem32real', 'mem64real', 'mem80real', 'mem80dec'}

	@staticmethod
	def is_mem_reg_opd(opd):
		""" Whether the operand is a ModRM register (integer or floating point) operand """
		return opd in {'Rb', 'Rw', 'Rd', 'Rv', 'Ry', 'ST(i)'}

	@staticmethod
	def is_reg_opd(opd):
		""" Whether the operand is a register operand """
		return opd in {'Cy', 'Dy', 'Gb', 'Gw', 'Gd', 'Gz', 'Gv', 'Sw', 'Ty'}

	@staticmethod
	def is_reg_only_opd(opd):
		""" Whether the operand is a register operand that ignores the MOD field """
		return opd in {'Cy', 'Dy', 'Ty'}

	@staticmethod
	def is_mem_or_reg_opd(opd):
		""" Whether the operand is a ModRM memory/register operand """
		return opd in {'E', 'Eb', 'Ew', 'Ed', 'Ez', 'Ev', 'Mw/Rv'}

	def check_if_has_modrm(self):
		if 'modrm' in self.kwds:
			return True
		for opd in self.kwds['opds']:
			if Instruction.is_reg_only_opd(opd):
				# For MOV Cy/Dy/Ty instructions
				self.kwds['modrm'] = False # don't parse modrm byte fully
				return True
		for opd in self.kwds['opds']:
			#if Instruction.is_reg_opd(opd) or Instruction.is_mem_reg_opd(opd):
			#	if 'modrm' not in self.kwds:
			#		self.kwds['modrm'] = False # if no memory operand, don't parse modrm byte fully
			if Instruction.is_mem_opd(opd) or Instruction.is_mem_or_reg_opd(opd) or Instruction.is_mem_reg_opd(opd):
				self.kwds['modrm'] = True
				return True
		return 'modrm' in self.kwds

	def separate_modfield_syntax(self, syntax = ''):
		opdsR = []
		opdsM = []
		for opd in self.kwds['opds'] if syntax == '' else self.kwds[syntax][1]:
			if Instruction.is_mem_reg_opd(opd):
				if opdsR is not None:
					opdsR.append(opd)
				opdsM = None
			elif Instruction.is_mem_opd(opd):
				opdsR = None
				if opdsM is not None:
					opdsM.append(opd)
			elif Instruction.is_mem_or_reg_opd(opd):
				if opdsR is not None:
					if '/' in opd:
						r = opd[opd.find('/') + 1:]
					elif opd == 'E':
						r = 'Rb'
					else:
						r = 'R' + opd[1:]
					opdsR.append(r)
				if opdsM is not None:
					if '/' in opd:
						m = opd[:opd.find('/')]
					else:
						m = 'M' + opd[1:]
					opdsM.append(m)
			else:
				if opdsR is not None:
					opdsR.append(opd)
				if opdsM is not None:
					opdsM.append(opd)
		return opdsR, opdsM

	def separate_modfield(self):
		opdsR, opdsM = self.separate_modfield_syntax()

		if opdsR == opdsM:
			return [self, self]

		alternatives = {}
		for syntax in ['nec', 'intel']:
			if syntax not in self.kwds:
				continue
			alternatives[syntax] = self.separate_modfield_syntax(syntax)

		result = []
		if opdsM is not None:
			kwds = self.kwds.copy()
			kwds['opds'] = opdsM
			for syntax, alternative in alternatives.items():
				assert alternatives[syntax][1] is not None
				kwds[syntax] = (kwds[syntax][0], alternatives[syntax][1])
			result.append(Instruction(**kwds))
		else:
			result.append(None)
		if opdsR is not None:
			kwds = self.kwds.copy()
			kwds['opds'] = opdsR
			for syntax, alternative in alternatives.items():
				assert alternatives[syntax][0] is not None
				kwds[syntax] = (kwds[syntax][0], alternatives[syntax][0])
			result.append(Instruction(**kwds))
		else:
			result.append(None)
		return result

class Entry:
	""" Represents a collection of instructions or tables corresponding to the same encoding, split between CPU types and feature flags """
	def __init__(self):
		# Every entry should contain at most one Table and any number of Instruction elements
		self.ins = []
		self.tab = None

	def deepcopy(self):
		entry = Entry()
		entry.ins = [ins.deepcopy() for ins in self.ins]
		entry.tab = entry.tab.deepcopy() if entry.tab is not None else None
		return entry

	def is_empty(self):
		return self.tab is None and len(self.ins) == 0

class Table:
	def __init__(self, size, **kwds):
		# Every entry should contain at most one Table and any number of Instruction elements
		self.entries = [Entry() for i in range(size)]
		self.kwds = kwds

	def deepcopy(self):
		copy = Table(len(self.entries), **self.kwds)
		for index, entry in enumerate(self.entries):
			if entry is not None:
				copy.entries[index] = entry.deepcopy()
		return copy

	def lookup(self, index):
		assert len(index) > 0
		if len(index) == 1:
			assert type(index[0][0]) is int
			return self.entries[index[0][0]]
		else:
			assert type(index[0][0]) is int
			table_type = index[1][1]

			subtable = self.entries[index[0][0]].tab
			if subtable is not None and subtable.kwds['discriminator'] == table_type:
				return subtable.lookup(index[1:])
			elif subtable.kwds['discriminator'] == 'modfield' and table_type == 'subtable':
				# if the step specifies an entire byte, split it up to MOD, REG, MEM fields
				assert (index[1][0] & 0xC0) == 0xC0
				newindex = Path((1, 'modfield'), ((index[1][0] >> 3) & 7, 'regfield'), (index[1][0] & 7, 'memfield')) + index[2:]
				return subtable.lookup(newindex)
			else:
				assert False

	def assign(self, index, value):
		assert len(index) > 0
		assert isinstance(value, Instruction)
		if len(index) == 1:
			assert type(index[0][0]) is int
			self.entries[index[0][0]].ins.append(value)
		else:
			assert type(index[0][0]) is int
			table_type = index[1][1]

			subtable = self.entries[index[0][0]].tab
			if subtable is None:
				self.entries[index[0][0]].tab = Table({'modfield': 2, 'regfield': 8, 'memfield': 8, 'subtable': 256, 'prefix': 4}[table_type], discriminator = table_type)
				subtable = self.entries[index[0][0]].tab
				subtable.assign(index[1:], value)
			elif subtable is not None and subtable.kwds['discriminator'] == table_type:
				subtable.assign(index[1:], value)
			elif subtable is not None and subtable.kwds['discriminator'] == 'modfield' and table_type == 'subtable':
				# if the step specifies an entire byte, split it up to MOD, REG, MEM fields
				assert (index[1][0] & 0xC0) == 0xC0
				newindex = Path((1, 'modfield'), ((index[1][0] >> 3) & 7, 'regfield'), (index[1][0] & 7, 'memfield')) + index[2:]
				subtable.assign(newindex, value)
			else:
				print(index, subtable.kwds['discriminator'], table_type)
				assert False

	def check_flags(self):
		"""Recursively check and build up flags/features and instruction set specifications"""
		self.kwds['isas'] = set()
		flags = None
		features = None
		for entry in self.entries:
			if entry.tab is not None:
				entry.tab.check_flags()
				self.kwds['isas'].update(entry.tab.kwds['isas'])
				if flags is None:
					flags = entry.tab.kwds['flags'].copy()
				else:
					flags.intersection_update(entry.tab.kwds['flags'])

				if features is None:
					features = entry.tab.kwds['features'].copy()
				else:
					features.intersection_update(entry.tab.kwds['features'])

			for ins in entry.ins:
				self.kwds['isas'].update(ins.kwds['isas'])
				if flags is None:
					flags = ins.kwds['flags'].copy()
				else:
					flags.intersection_update(ins.kwds['flags'])

				if features is None:
					features = ins.kwds['features'].copy()
				else:
					features.intersection_update(ins.kwds['features'])

		self.kwds['flags'] = flags
		if features is None:
			features = set()
		self.kwds['features'] = features

	def check_if_has_modrm(self):
		"""Recursively check if ModRM byte is required"""
		for entry in self.entries:
			if entry.tab is not None:
				entry.tab.check_if_has_modrm()
			for ins in entry.ins:
				ins.check_if_has_modrm()

		if self.kwds.get('discriminator') in {'modfield', 'regfield', 'memfield'}:
			self.kwds['modrm'] = False
			for entry in self.entries:
				for item in ([entry.tab] if entry.tab is not None else []) + entry.ins:
					if 'modrm' in item.kwds:
						self.kwds['modrm'] |= item.kwds['modrm']
						item.kwds['modrm'] = False

		elif self.kwds.get('discriminator') == 'prefix':
			for entry in self.entries:
				for item in ([entry.tab] if entry.tab is not None else []) + entry.ins:
					if 'modrm' in item.kwds:
						if 'modrm' not in self.kwds or item.kwds['modrm'] == True:
							self.kwds['modrm'] = item.kwds['modrm']
						item.kwds['modrm'] = False

		return 'modrm' in self.kwds

	def check_prefix_defaults(self, fallbacks = None):
		if self.kwds['discriminator'] == 'prefix':
			if fallbacks is None:
				return
			for entry in self.entries:
				assert entry.tab is None
				new_ins = []
				for fallback in fallbacks:
					isas = self.kwds['isas'].intersection(fallback.kwds['isas'])
					for ins in entry.ins:
						isas.difference_update(self.kwds['isas'].intersection(fallback.kwds['isas']))
					if len(isas) != 0:
						fallback1 = fallback.deepcopy()
						fallback1.kwds['isas'] = isas
						new_ins.append(fallback1)
				entry.ins += new_ins
		else:
			for entry in self.entries:
				if entry.tab is not None:
					assert fallbacks is None
					if entry.tab.kwds['discriminator'] == 'prefix':
						entry.tab.check_prefix_defaults(entry.ins.copy())
						for ins in entry.ins:
							ins.kwds['isas'].difference_update(entry.tab.kwds['isas'])
					else:
						entry.tab.check_prefix_defaults()

def is_instruction_exclusive(opds):
	"""Whether only memory or register operands are accepted for the ModRM byte, returns 'M' or 'R' or None"""
	exclusive = None
	for opd in opds:
		if Instruction.is_mem_opd(opd):
			assert exclusive is None
			exclusive = 'M'
		elif Instruction.is_mem_reg_opd(opd):
			assert exclusive is None
			exclusive = 'R'
		elif Instruction.is_mem_or_reg_opd(opd):
			assert exclusive is None
			exclusive = ''
	if exclusive == '':
		exclusive = None
	return exclusive

def z80_add_prefixed(code, ins, prefix, replacements, extra_opds = None):
	global X80_TABLE
	ins = ins.deepcopy()
	ins.kwds['isas'] = {'z80'}
	if len(replacements) > 0:
		ins.kwds['opds'] = tuple(replacements.get(opd, opd) for opd in ins.kwds['opds'])
	if extra_opds is not None:
		ins.kwds['opds'] = ins.kwds['opds'] + extra_opds
	if 'intel' in ins.kwds:
		del ins.kwds['intel']
	code = Path((prefix, 'subtable')) + code
	X80_TABLE.assign(code, ins)

def read_data(filename):
	"""
		The input file is a collection of sections with different formats

		The @architectues section lists major architectural groups that identify groups of instructions and behavior
		The
	"""
	global ARCHITECTURES, X80_ARCHITECTURE_LIST, X86_ARCHITECTURE_LIST, X87_ARCHITECTURE_LIST
	global X80_TABLE, X86_TABLE, X87_TABLE
	global INSTRUCTIONS
	global FEATURE_NAMES
	global PROCESSORS

	opcodes_text = ''
	instructions_raw = {}
	architectures_text = ''
	features_text = ''
	processors_text = ''

	section_name = None
	instruction_name = None
	with open(filename, 'r') as fp:
		for num, line in enumerate(fp):
			if line.startswith('@instruction '):
				section_name = '@instruction'
				instruction_name = line[13:].strip()
				instructions_raw[instruction_name] = [num + 2, filename, ""]
			elif line.startswith('@comment '):
				continue
			elif line.rstrip() in {'@instructionset', '@architectures', '@features', '@processors'}:
				section_name = line.rstrip()
			elif section_name == '@instruction':
				instructions_raw[instruction_name][-1] += line
			elif section_name == '@instructionset':
				opcodes_text += line
			elif section_name == '@architectures':
				architectures_text += line
			elif section_name == '@features':
				features_text += line
			elif section_name == '@processors':
				processors_text += line

	FEATURE_NAMES = {}
	for line in features_text.split('\n'):
		if '#' in line:
			line = line[:line.index('#')]
		line = line.strip()
		if line == '':
			continue
		name, description = line.split('\t')
		FEATURE_NAMES[name] = description

	PROCESSORS = yaml.load(processors_text, Loader = yaml.loader.BaseLoader)

	_ARCHITECTURES = yaml.load(architectures_text, Loader = yaml.loader.BaseLoader)
	ARCHITECTURE_LIST = [isa['id'] for isa in _ARCHITECTURES]
	ARCHITECTURES = {isa['id']: isa for isa in _ARCHITECTURES}
	X80_ARCHITECTURE_LIST = [isa for isa in ARCHITECTURE_LIST if ARCHITECTURES[isa].get('type', 'cpu') == '8bit']
	X86_ARCHITECTURE_LIST = [isa for isa in ARCHITECTURE_LIST if ARCHITECTURES[isa].get('type', 'cpu') == 'cpu']
	X87_ARCHITECTURE_LIST = [isa for isa in ARCHITECTURE_LIST if ARCHITECTURES[isa].get('type', 'cpu') == 'fpu']

	INSTRUCTIONS = {}
	for name, (line, file, value) in instructions_raw.items():
		parts = name.split('|')
		name = parts.pop(0).strip()
		value = value.strip()
		if name not in INSTRUCTIONS:
			INSTRUCTIONS[name] = [(parts, (line, file, value))]
		else:
			INSTRUCTIONS[name].append((parts, (line, file, value)))

	X80_TABLE = Table(256, discriminator = 'subtable')
	X86_TABLE = Table(256, discriminator = 'subtable')
	X87_TABLE = Table(256, discriminator = 'subtable')

	for line in opcodes_text.split('\n'):
		if '#' in line:
			line = line[:line.index('#')]
		line = line.strip()
		if line == "":
			continue

		parts = line.split('\t')
		code = parts.pop(0)
		isaspec = parts.pop(0)

		mnem_opds = {}
		current_syntax = ''
		feature_text = None
		while len(parts) > 0:
			if current_syntax in mnem_opds:
				print("Invalid entry: ", parts[0], file = sys.stderr)

			mnem = parts.pop(0)
			if len(parts) == 0:
				opds = ()
			elif parts[0] == '-':
				opds = ()
				parts.pop(0)
			else:
				opds = tuple(opd.strip() for opd in parts.pop(0).split(','))
			mnem_opds[current_syntax] = mnem, opds
			while len(parts) > 1 and parts[0] == '':
				parts.pop(0)
			if len(parts) > 1:
				if parts[0] in {'nec:', 'intel:'}:
					current_syntax = parts.pop(0)[:-1]
				elif parts[0] == 'feature:':
					assert feature_text is None
					parts.pop(0)
					feature_text = parts.pop(0)
				else:
					print("Unknown flag: ", repr(parts[0]), file = sys.stderr)
					break

		if feature_text is not None:
			features = set()
			for feature_name in feature_text.split(','):
				negated = False
				if feature_name.startswith('!'):
					feature_name = feature_name[1:]
					negated = True
				if feature_name not in FEATURE_NAMES:
					print("Undefined feature: ", feature_name, file = sys.stderr)
					continue
				else:
					if negated:
						feature_name = ('!', feature_name)
					features.add(feature_name)
		else:
			features = None

		isaspec = {isa.strip() for isa in isaspec.split(',')} if isaspec != '' else set()
		is8bit = False
		is_fpu = False
		for isa in isaspec.copy():
			if isa.endswith('+'):
				isa0 = isa[:-1]
			elif isa.startswith('!'):
				isa0 = isa[1:]
			else:
				isa0 = isa

			if isa0 not in ARCHITECTURE_LIST and isa0 != '64':
				print("Unknown ISA:", isa0, file = sys.stderr)

			if isa0 in X80_ARCHITECTURE_LIST:
				is8bit = True
			if isa0 in X87_ARCHITECTURE_LIST:
				is_fpu = True

		mnem, opds = mnem_opds['']
		alternatives = mnem_opds.copy()
		del alternatives['']
		ins = Instruction(mnem = mnem, opds = opds, **alternatives)
		if features is None:
			features = set()
		ins.kwds['features'] = features

		if not is8bit:
			if '/' in code:
				code, regfield = code.split('/')
				regfield = int(regfield, 16)
			else:
				regfield = None

			prefix = None
			if code.startswith('NP'):
				code = code[2:].strip()
				prefix = (0, 'prefix')

			# since 0x00 is not an instruction prefix in either x80 or x86, we may treat opcode sequences as a (little endian) hexadecimal word
			index = int(code, 16)
			code = []
			while True:
				if index < 256:
					code.append((index, 'subtable'))
					break
				else:
					shift = 8
					while (index >> shift) != 0:
						high = index >> shift
						low = index & ((1 << shift) - 1)
						shift += 8
					if high in {0x66, 0xF2, 0xF3} and len(code) == 0:
						assert prefix is None
						index = low
						prefix = ([None, 0x66, 0xF3, 0xF2].index(high), 'prefix')
					else:
						code.append((high, 'subtable'))
						index = low

			if regfield is not None:
				code.append((regfield, 'regfield'))

			exclusive = is_instruction_exclusive(opds)
			if exclusive is not None:
				# Note: modfield will be reordered
				code.append((['M', 'R'].index(exclusive), 'modfield'))

			if prefix is not None:
				code.append(prefix)

			code = Path(*code)

			suppisas = set()

			# instruction sets that include this instruction in their successors as well
			predisas = set()
			for isa in X86_ARCHITECTURE_LIST if not is_fpu else X87_ARCHITECTURE_LIST:
				if '!' + isa in isaspec:
					continue

				ignore_for_this_cpu = '64' in isaspec and '64' not in ARCHITECTURES[isa].get('modes', ())

				if isa + '+' in isaspec or any(_isa in predisas for _isa in ARCHITECTURES[isa].get('predecessors', ())):
					predisas.add(isa)
					if not ignore_for_this_cpu:
						suppisas.add(isa)
				elif isa in isaspec:
					if not ignore_for_this_cpu:
						suppisas.add(isa)
				else:
					continue

			flags = set()

			if '64' in isaspec:
				flags.add('only64bit')
			if '!64' in isaspec:
				flags.add('only32bit')

			ins.kwds['flags'] = flags
			ins.kwds['isas'] = suppisas

			if ins.check_if_has_modrm():
				for modfield, ins0 in enumerate(ins.separate_modfield()):
					if ins0 is not None:
						if is_fpu:
							X87_TABLE.assign(code.separate_modfield(modfield), ins0)
						else:
							X86_TABLE.assign(code.separate_modfield(modfield), ins0)
			else:
				if is_fpu:
					X87_TABLE.assign(code, ins)
				else:
					X86_TABLE.assign(code, ins)

		else:
			# since 0x00 is not an instruction prefix in either x80 or x86, we may treat opcode sequences as a (little endian) hexadecimal word
			index = int(code, 16)
			code = []
			while True:
				if index < 256:
					code.append((index, 'subtable'))
					break
				else:
					shift = 8
					while (index >> shift) != 0:
						high = index >> shift
						index &= ((1 << shift) - 1)
						shift += 8
					code.append((high, 'subtable'))

			code = Path(*code)

			suppisas = set()
			predisas = set()
			for isa in X80_ARCHITECTURE_LIST:
				if '!' + isa in isaspec:
					continue

				if isa + '+' in isaspec or any(_isa in predisas for _isa in ARCHITECTURES[isa].get('predecessors', ())):
					predisas.add(isa)
					# inclisas.add(isa) # not currently used
					suppisas.add(isa)
				elif isa in isaspec: # or any(_isa in inclisas for _isa in ARCHITECTURES[isa].get('include', ())): # not currently used
					pass # inclisas.add(isa) # not currently used
					suppisas.add(isa)
				else:
					continue

			ins.kwds['flags'] = flags
			ins.kwds['isas'] = suppisas
			X80_TABLE.assign(code, ins)

			if 'z80' in suppisas and len(code) == 1:
				if '(HL)' in ins.kwds['opds']:
					z80_add_prefixed(code, ins, 0xDD, {'(HL)': '(IX+Ib)'})
					z80_add_prefixed(code, ins, 0xFD, {'(HL)': '(IY+Ib)'})
				elif 'H' in opds or 'L' in opds or 'HL' in opds and not (mnem == 'EX' and opds == ('DE', 'HL')):
					z80_add_prefixed(code, ins, 0xDD, {'H':'IXH','L':'IXL','HL':'IX'})
					z80_add_prefixed(code, ins, 0xFD, {'H':'IYH','L':'IYL','HL':'IY'})
				elif mnem == 'bits':
					z80_add_prefixed(code, ins, 0xDD, {}, ('(IX+Ib)',))
					z80_add_prefixed(code, ins, 0xFD, {}, ('(IY+Ib)',))
				else:
					z80_add_prefixed(code, ins, 0xDD, {})
					z80_add_prefixed(code, ins, 0xFD, {})

	X80_TABLE.check_flags()
	X86_TABLE.check_flags()
	X86_TABLE.check_prefix_defaults()
	X86_TABLE.check_if_has_modrm()
	X87_TABLE.check_flags()
	X87_TABLE.check_if_has_modrm() # Note: these all have modrm

def print_table(table, path = Path()):
	for index, entry in enumerate(table.entries):
		path1 = path + Path((index, table.kwds['discriminator']))
		for ins in entry.ins:
			print(path1, ins)
		if entry.tab is not None:
			print(path1, ">", entry.tab.kwds['discriminator'], sorted(entry.tab.kwds.items()))
			print_table(entry.tab, path1)

read_data(sys.argv[1])

#print_table(X80_TABLE)
#print_table(X86_TABLE)
#print_table(X87_TABLE)
#print("===")

file_line = 1
def print_file(*parts, file = None):
	global file_line
	assert file is not None
	parts = ''.join(map(str, parts))
	#print(f'/* {file_line} */', parts, file = file)
	print(parts, file = file)
	file_line += parts.count('\n') + 1

# x86 operand details

OPERAND_CODE = {
	'1': {
		'size':    '',
		'read':    "1",
		'format':  ("1", []),
	},
	'Ib': {
		'size':    'b',
		'prepare': 'soff_t imm$# = (int8_t)x86_fetch8(prs, emu);',
		'read':    "imm$#",
		'write':   "imm$# = $$",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Ub': {
		'size':    'b',
		'prepare': 'soff_t imm$# = (uint8_t)x86_fetch8(prs, emu);',
		'read':    "imm$#",
		'write':   "imm$# = $$",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Iw': {
		'size':    'w',
		'prepare': 'soff_t imm$# = (int16_t)x86_fetch16(prs, emu);',
		'read':    "imm$#",
		'write':   "imm$# = $$",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Iz': {
		'size':    'wl',
		'prepare': 'soff_t imm$# = (int$bits$_t)x86_fetch$bits$(prs, emu);',
		'read':    "imm$#",
		'write':   "imm$# = $$",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Iv': {
		'size':    'wlq',
		'prepare': 'soff_t imm$# = (int$bits$_t)x86_fetch$bits$(prs, emu);',
		'read':    "imm$#",
		'write':   "imm$# = $$",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Ap': {
		'size':    'wl',
		'prepare': 'soff_t imm$# = (int$bits$_t)x86_fetch$bits$(prs, emu);\nsoff_t seg$# = x86_fetch16(prs, emu);',
		'read':    "imm$#",
		'read2':   "seg$#",
		'write':   "imm$# = $$",
		'write2':  "seg$# = $$",
		'format':  ('%"PRIX64 ":%"PRIX64"', ["(uoff_t)seg$#", "(uoff_t)imm$#"]),
	},
	'Jb': {
		'size':    'b',
		'prepare': 'soff_t imm$# = (int8_t)x86_fetch8(prs, emu);',
		'read':    "(imm$# + emu->xip)",
		'format':  ('%"PRIX64"', ["(uoff_t)(imm$# + prs->current_position)"]),
	},
	'Jz': {
		'size':    'wl',
		'prepare': 'soff_t imm$# = (int$bits$_t)x86_fetch$bits$(prs, emu);',
		'read':    "(imm$# + emu->xip)",
		'format':  ('%"PRIX64"', ["(uoff_t)(imm$# + prs->current_position)"]),
	},
	'Ov': {
		'size':    '',
		'prepare': 'prs->address_offset = x86_fetch_addrsize(prs, emu);',
		'address': "emu->parser->address_offset",
		'read':    "_read$S(_seg == X86_R_SEGMENT_NONE ? X86_R_DS : _seg, _off$?)",
		'write':   "_write$S(_seg == X86_R_SEGMENT_NONE ? X86_R_DS : _seg, _off$?, $$)",
		'format':  ('[%s:%"PRIX64"]', ["x86_segment_name(prs, _seg == X86_R_SEGMENT_NONE ? X86_R_DS : _seg)", "prs->address_offset"]),
	},
	'AL': {
		'size':    'b',
		'read':    "x86_register_get8_low(emu, X86_R_AX)",
		'write':   "x86_register_set8_low(emu, X86_R_AX, $$)",
		'format':  ("al", []),
	},
	'CL': {
		'size':    'b',
		'read':    "x86_register_get8_low(emu, X86_R_CX)",
		'write':   "x86_register_set8_low(emu, X86_R_CX, $$)",
		'format':  ("cl", []),
	},
	'AX': {
		'size':    'w',
		'read':    "x86_register_get16(emu, X86_R_AX)",
		'write':   "x86_register_set16(emu, X86_R_AX, $$)",
		'format':  ("%s", ["x86_is_nec(prs) ? \"aw\" : \"ax\""]),
	},
	'DX': {
		'size':    'w',
		'read':    "x86_register_get16(emu, X86_R_DX)",
		'write':   "x86_register_set16(emu, X86_R_DX, $$)",
		'format':  ("%s", ["x86_is_nec(prs) ? \"dw\" : \"dx\""]),
	},
	'rAX': {
		'size':    'wlq',
		'read':    "x86_register_get$S(emu, X86_R_AX)",
		'write':   "x86_register_set$S(emu, X86_R_AX, $Sc$$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, X86_R_AX)"]),
	},
	'Gb': {
		'size':    'b',
		'read':    "x86_register_get8(emu, _reg)",
		'write':   "x86_register_set8(emu, _reg, $$)",
		'format':  ("%s", ["x86_register_name8(prs, REGFLD(prs))"]),
	},
	'Gw': {
		'size':    'w',
		'read':    "x86_register_get16(emu, _reg)",
		'write':   "x86_register_set16(emu, _reg, $$)",
		'format':  ("%s", ["x86_register_name16(prs, REGFLD(prs))"]),
	},
	'Gz': {
		'size':    'wl',
		'read':    "x86_register_get$S(emu, _reg)",
		'write':   "x86_register_set$S(emu, _reg, $Sc$$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, REGFLD(prs))"]),
	},
	'Gv': {
		'size':    'wlq',
		'read':    "x86_register_get$S(emu, _reg)",
		'write':   "x86_register_set$S(emu, _reg, $Sc$$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, REGFLD(prs))"]),
	},
	'Rb': {
		'size':    'b',
		'read':    "x86_register_get8(emu, _mem)",
		'write':   "x86_register_set8(emu, _mem, $$)",
		'format':  ("%s", ["x86_register_name8(prs, MEMFLD(prs))"]),
	},
	'Rw': {
		'size':    'w',
		'read':    "x86_register_get16(emu, _mem)",
		'write':   "x86_register_set16(emu, _mem, $$)",
		'format':  ("%s", ["x86_register_name16(prs, MEMFLD(prs))"]),
	},
	'Rd': {
		'size':    'l',
		'read':    "x86_register_get32(emu, _mem)",
		'write':   "x86_register_set32(emu, _mem, $$)",
		'format':  ("%s", ["x86_register_name32(prs, MEMFLD(prs))"]),
	},
	'Rz': {
		'size':    'wl',
		'read':    "x86_register_get$S(emu, _mem)",
		'write':   "x86_register_set$S(emu, _mem, $Sc$$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, MEMFLD(prs))"]),
	},
	'Rv': {
		'size':    'wlq',
		'read':    "x86_register_get$S(emu, _mem)",
		'write':   "x86_register_set$S(emu, _mem, $Sc$$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, MEMFLD(prs))"]),
	},
	'Ry': {
		'size':    'lq',
		'read':    "x86_register_get32(emu, _mem)",
		'write':   "x86_register_set32(emu, _mem, $$)",
		'format':  ("%s", ["x86_register_name$bits$(prs, MEMFLD(prs))"]),
	},
	'Sw': {
		'size':    'w',
		'read':    "x86_segment_get(emu, REGFLDVAL(prs))",
		'write':   "x86_segment_set(emu, REGFLDVAL(prs), $$)",
		'format':  ("%s", ["x86_segment_name(prs, REGFLDVAL(prs))"]),
	},
	'Cy': {
		'size':    'lq',
		'read':    "_crget$S(REGFLDLOCK(prs))",
		'write':   "_crset$S(REGFLDLOCK(prs), $$)",
		'format':  ("cr%d", ["REGFLDLOCK(prs)"]),
	},
	'Dy': {
		'size':    'lq',
		'read':    "_drget$S(REGFLDLOCK(prs))",
		'write':   "_drset$S(REGFLDLOCK(prs), $$)",
		'format':  ("dr%d", ["REGFLDLOCK(prs)"]),
	},
	'Ty': {
		'size':    'lq',
		'read':    "_trget32(REGFLDLOCK(prs))",
		'write':   "_trset32(REGFLDLOCK(prs), $$)",
		'format':  ("tr%d", ["REGFLDLOCK(prs)"]), # Note: 386/486 only uses 8 TR registers, but we can assume the LOCK prefix would be extended to them
	},
	'M': {
		'size':    '',
		'address': "emu->parser->address_offset",
		'format':  ("%s", ["prs->address_text"]),
	},
	'Mb': {
		'size':    'b',
		'address': "emu->parser->address_offset",
		'read':    "(_int8)_read8(_seg, _off$?)",
		'write':   "_write8(_seg, _off$?, $$)",
		'format':  ("byte %s", ["prs->address_text"]),
	},
	'Mw': {
		'size':    'w',
		'address': "emu->parser->address_offset",
		'read':    "(_int16)_read16(_seg, _off$?)",
		'write':   "_write16(_seg, _off$?, $$)",
		'format':  ("word %s", ["prs->address_text"]),
	},
	'Md': {
		'size':    'l',
		'address': "emu->parser->address_offset",
		'read':    "(_int32)_read32(_seg, _off$?)",
		'write':   "_write32(_seg, _off$?, $$)",
		'format':  ("long %s", ["prs->address_text"]),
	},
	'Mz': {
		'size':    'wl',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("$size$ %s", ["prs->address_text"]),
	},
	'Mv': {
		'size':    'wlq',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("$size$ %s", ["prs->address_text"]),
	},
	'Mz': {
		'size':    'wl',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("$size$ %s", ["prs->address_text"]),
	},
	'Ma': {
		'size':    'wl',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("%s", ["prs->address_text"]),
	},
	'Mq/Mo': {
		'size':    'lq',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("%s", ["prs->address_text"]),
	},
	'Mp': {
		'size':    'wlq',
		'address': "emu->parser->address_offset",
		'read':    "(_int$S)_read$S(_seg, _off$?)",
		'write':   "_write$S(_seg, _off$?, $$)",
		'format':  ("%s", ["prs->address_text"]),
	},

# x87
	'ST': {
		'size':    '',
		'read':    "x87_register_get80(emu, 0)",
		'write':   "x87_register_set80(emu, 0, $$)",
		'format':  ("st", []),
	},
	'ST(i)': {
		'size':    '',
		'read':    "x87_register_get80(emu, _mem)",
		'write':   "x87_register_set80(emu, _mem, $$)",
		'format':  ("st(%d)", ["MEMFLD(prs)"]),
	},
	'mem32real': {
		'size':    '',
		'read':    "x87_float32_to_float80(emu, _read32fp(_seg, _off))",
		'write':   "_write32fp(_seg, _off, x87_float80_to_float32(emu, $$))",
		'format':  ("%s", ["emu ? emu->x87.address_text : prs->address_text"]),
	},
	'mem64real': {
		'size':    '',
		'read':    "x87_float64_to_float80(emu, _read64fp(_seg, _off))",
		'write':   "_write64fp(_seg, _off, x87_float80_to_float64(emu, $$))",
		'format':  ("%s", ["emu ? emu->x87.address_text : prs->address_text"]),
	},
	'mem80real': {
		'size':    '',
		'read':    "_read80fp(_seg, _off)",
		'write':   "_write80fp(_seg, _off, $$)",
		'format':  ("%s", ["emu ? emu->x87.address_text : prs->address_text"]),
	},
	'M14/M28': {
		'size':    '',
		'format':  ("%s", ["emu ? emu->x87.address_text : prs->address_text"]),
	},
	'M94/M108': {
		'size':    '',
		'format':  ("%s", ["emu ? emu->x87.address_text : prs->address_text"]),
	},

# NEC exclusive
	# used for int3/brk 3
	'3': {
		'size':    '',
		'format':  ("3", []),
	},
	# used for pusha/push r and popa/pop r
	'R': {
		'size':    '',
		'format':  ("r", []),
	},
	# used for pushf/push psw and popf/pop psw
	'PSW': {
		'size':    '',
		'format':  ("psw", []),
	},
	# used for lahf/mov ah, psw and sahf/mov psw, ah
	'AH': {
		'size':    '',
		'format':  ("ah", []),
	},
	# used for lds R, M/mov ds0, R, M
	'DS0': {
		'size':    '',
		'format':  ("ds0", []),
	},
	# used for les R, M/mov ds1, R, M
	'DS1': {
		'size':    '',
		'format':  ("ds1", []),
	},
	# used for stc/set1 cy, clc/clr1 cy and cmc/not1 cy
	'CY': {
		'size':    '',
		'format':  ("cy", []),
	},
	# used for std/set1 dir and cld/clr1 dir
	'DIR': {
		'size':    '',
		'format':  ("dir", []),
	},
}

WREGS = ['AX', 'CX', 'DX', 'BX', 'SP', 'BP', 'SI', 'DI']
SREGS = ['ES', 'CS', 'SS', 'DS', 'FS', 'GS', 'DS3', 'DS2']

for i in range(8):
	OPERAND_CODE[f'r{i}'] = {
		'size':    'wlq',
		'read':    f"x86_register_get$S(emu, REGNUM(prs, {i}))",
		'write':   f"x86_register_set$S(emu, REGNUM(prs, {i}), $Sc$$)",
		'format':  ("%s", [f"x86_register_name$bits$(prs, REGNUM(prs, {i}))"]),
	}
	OPERAND_CODE[f'R{i}B'] = {
		'size':    'b',
		'read':    f"x86_register_get8(emu, REGNUM(prs, {i}))",
		'write':   f"x86_register_set8(emu, REGNUM(prs, {i}), $$)",
		'format':  ("%s", [f"x86_register_name8(prs, REGNUM(prs, {i}))"]),
	}
	OPERAND_CODE['e' + WREGS[i]] = {
		'size':    'wl',
		'read':    f"x86_register_get16(emu, REGNUM(prs, {i}))",
		'write':   f"x86_register_set16(emu, REGNUM(prs, {i}), $$)",
		'format':  ("%s", [f"x86_register_name$bits$(prs, X86_R_{WREGS[i]})"]),
	}
	if i < len(SREGS):
		OPERAND_CODE[SREGS[i]] = {
			'size':    'w',
			'read':    f"emu->sr[{i}].selector",
			'write':   f"x86_segment_set(emu, X86_R_{SREGS[i]}, $$)",
			'format':  ("%s", [f"x86_segment_name(prs, X86_R_{SREGS[i]})"]),
		}

OPERAND_CODE['M10'] = OPERAND_CODE['M']

# x87 operand details

def operand_convert_x87(value):
	if type(value) is str:
		return value.replace('_read', '_x87_read').replace('_write', '_x87_write')
	elif type(value) is tuple:
		return tuple(map(operand_convert_x87, value))
	elif type(value) is list:
		return list(map(operand_convert_x87, value))
	elif type(value) is dict:
		return {akey: operand_convert_x87(avalue) for akey, avalue in value.items()}
	else:
		return value

X87_OPERAND_CODE = operand_convert_x87(OPERAND_CODE)

X80_OPERAND_CODE = {
	'Ib': {
		'prepare': 'soff_t imm$# = (int8_t)x80_fetch8(prs, emu);',
		'read':    "imm$#",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Iw': {
		'prepare': 'soff_t imm$# = (int16_t)x80_fetch16(prs, emu);',
		'read':    "imm$#",
		'format':  ('%"PRIX64"', ["(uoff_t)imm$#"]),
	},
	'Jb': {
		'prepare': 'soff_t imm$# = (int8_t)x80_fetch8(prs, emu);',
		'read':    "imm$# + (emu->pc & 0xFFFF)",
		'format':  ('%"PRIX64"', ["(uoff_t)(imm$# + (prs->current_position & 0xFFFF))"]),
	},
	'A': {
		'read':   "x86_get_high(emu->bank[emu->af_bank].af)",
		'write':  "x86_set_high(&emu->bank[emu->af_bank].af, $$)",
		'format':  ("a", []),
	},
	'B': {
		'read':   "x86_get_high(emu->bank[emu->main_bank].bc)",
		'write':  "x86_set_high(&emu->bank[emu->main_bank].bc, $$)",
		'format':  ("b", []),
	},
	'C': {
		'read':   "x86_get_low(emu->bank[emu->main_bank].bc)",
		'write':  "x86_set_low(&emu->bank[emu->main_bank].bc, $$)",
		'condition': "C",
		'format':  ("c", []),
	},
	'D': {
		'read':   "x86_get_high(emu->bank[emu->main_bank].bc)",
		'write':  "x86_set_high(&emu->bank[emu->main_bank].bc, $$)",
		'format':  ("d", []),
	},
	'E': {
		'read':   "x86_get_low(emu->bank[emu->main_bank].bc)",
		'write':  "x86_set_low(&emu->bank[emu->main_bank].bc, $$)",
		'format':  ("e", []),
	},
	'H': {
		'read':   "x86_get_high(emu->bank[emu->main_bank].hl)",
		'write':  "x86_set_high(&emu->bank[emu->main_bank].hl, $$)",
		'format':  ("h", []),
	},
	'L': {
		'read':   "x86_get_low(emu->bank[emu->main_bank].hl)",
		'write':  "x86_set_low(&emu->bank[emu->main_bank].hl, $$)",
		'format':  ("l", []),
	},
	'IXH': {
		'read':   "x86_get_high(emu->ix)",
		'write':  "x86_set_high(&emu->ix, $$)",
		'format':  ("ixh", []),
	},
	'IXL': {
		'read':   "x86_get_low(emu->ix)",
		'write':  "x86_set_low(&emu->ix, $$)",
		'format':  ("ixl", []),
	},
	'IYH': {
		'read':   "x86_get_high(emu->iy)",
		'write':  "x86_set_high(&emu->iy, $$)",
		'format':  ("iyh", []),
	},
	'IYL': {
		'read':   "x86_get_low(emu->iy)",
		'write':  "x86_set_low(&emu->iy, $$)",
		'format':  ("iyl", []),
	},
	'AF': {
		'read':   "emu->bank[emu->af_bank].af",
		'write':  "emu->bank[emu->af_bank].af = $$",
		'format': ("af", []),
	},
	'BC': {
		'read':   "emu->bank[emu->main_bank].bc",
		'write':  "emu->bank[emu->main_bank].bc = $$",
		'format': ("bc", []),
	},
	'DE': {
		'read':   "emu->bank[emu->main_bank].de",
		'write':  "emu->bank[emu->main_bank].de = $$",
		'format': ("de", []),
	},
	'HL': {
		'read':   "emu->bank[emu->main_bank].hl",
		'write':  "emu->bank[emu->main_bank].hl = $$",
		'format': ("hl", []),
	},
	'SP': {
		'read':   "emu->sp",
		'write':  "emu->sp = $$",
		'format':  ("sp", []),
	},
	'IX': {
		'read':   "emu->ix",
		'write':  "emu->ix = $$",
		'format':  ("ix", []),
	},
	'IY': {
		'read':   "emu->iy",
		'write':  "emu->iy = $$",
		'format':  ("iy", []),
	},
	'(Iw)': {
		'prepare': 'soff_t imm$# = (int16_t)x80_fetch16(prs, emu);',
		'read':    "_read80b(imm$#)",
		'write':   "_write80b(imm$#, $$)",
		'format':  ('(%"PRIX64")', ["(uoff_t)imm$#"]),
	},
	'(BC)': {
		'read':    "_read80b(emu->bank[emu->main_bank].bc)",
		'write':   "_write80b(emu->bank[emu->main_bank].bc, $$)",
		'format':  ("(bc)", []),
	},
	'(DE)': {
		'read':    "_read80b(emu->bank[emu->main_bank].de)",
		'write':   "_write80b(emu->bank[emu->main_bank].de, $$)",
		'format':  ("(de)", []),
	},
	'(HL)': {
		'read':    "_read80b(emu->bank[emu->main_bank].hl)",
		'write':   "_write80b(emu->bank[emu->main_bank].hl, $$)",
		'format':  ("(hl)", []),
	},
	'(SP)': {
		'read':    "_read80b(emu->sp)",
		'write':   "_write80b(emu->sp, $$)",
		'format':  ("(sp)", []),
	},
	'(Ib)': {
		'prepare': 'soff_t imm$# = x80_fetch8(prs, emu) & 0xFF;',
		'read':    "(emu->bank[emu->af_bank].af & 0xFF00) | imm$#",
		'format':  ('(%"PRIX64")', ["(uoff_t)imm$#"]),
	},
	'(IX+Ib)': {
		'prepare': 'soff_t imm$# = (int8_t)x80_fetch8(prs, emu);',
		'read':    "_read80b(emu->ix + imm$#)",
		'write':   "_write80b(emu->ix + imm$#, $$)",
		'format':  ('(ix+%"PRIX64")', ["(uoff_t)imm$#"]),
	},
	'(IY+Ib)': {
		'prepare': 'soff_t imm$# = (int8_t)x80_fetch8(prs, emu);',
		'read':    "_read80b(emu->iy + imm$#)",
		'write':   "_write80b(emu->iy + imm$#, $$)",
		'format':  ('(ix+%"PRIX64")', ["(uoff_t)imm$#"]),
	},
	'(C)': {
		'read':    "emu->bank[emu->main_bank].bc", # Z80 actually uses BC
		'format':  ("(c)", []),
	},
	'I': {
		'read':   "emu->i",
		'write':  "emu->i = $$",
		'format':  ("i", []),
	},
	'R': {
		'read':   "emu->r",
		'write':  "emu->r = $$",
		'format':  ("r", []),
	},
	"AF'": {
		# dummy parameter
		'format':  ("af'", []),
	},

	'Z': {
		'condition': 'Z',
		'format':  ('z', []),
	},
	'NZ': {
		'condition': 'NZ',
		'format':  ('nz', []),
	},
	'NC': {
		'condition': 'NC',
		'format':  ('nc', []),
	},
	'PE': {
		'condition': 'PE',
		'format':  ('pe', []),
	},
	'PO': {
		'condition': 'PO',
		'format':  ('po', []),
	},
	'M': {
		'condition': 'M',
		'format':  ("m", []),
	},
	'P': {
		'condition': 'P',
		'format':  ('p', []),
	},
}

for i in range(8):
	X80_OPERAND_CODE[str(i)] = {'read': str(i), 'format': (str(i), [])}
for i in range(0, 0x40, 8):
	X80_OPERAND_CODE[f'{i:02X}h'] = {'read': str(i), 'format': (f'{i:02X}h', [])}

SIZE_NAMES = {'b': 'byte', 'w': 'word', 'l': 'long', 'q': 'quad', 'v': 'void'}

def max_long(size):
	return size if size != 'quad' else 'long'

def get_bits(size):
	return {'byte': 8, 'word': 16, 'long': 32, 'quad': 64, 'void': 0}[size]

def get_bytes(size):
	return get_bits(size) >> 3

def replace_patterns(text, size, sizes, **kwds):
	# Fixes operand size
	assert type(text) is str
	if size[0] not in sizes and len(sizes) == 1:
		size = SIZE_NAMES[sizes[0]]
	if size[0] not in sizes and size == 'byte':
		size = 'word'
	if size[0] not in sizes and size == 'word':
		size = 'long'
	if size[0] not in sizes:
		if size == 'long':
			if 'q' in sizes:
				size = 'quad'
			else:
				size = 'word'
		elif size == 'quad':
			size = 'long'
	return text.replace('$size$', size).replace('$bits$', str(get_bits(size)))

def iterate_sizes(sizes, indent = '\t', file = None, size_option = None):
	assert len(sizes) > 0
	if len(sizes) == 1:
		yield indent, SIZE_NAMES[next(iter(sizes))]
	elif len(sizes) > 1:
		if size_option == 'control':
			print_file(f"{indent}if(prs->code_size == SIZE_64BIT)", file = file)
			print_file(f"{indent}{{", file = file)
			yield indent + '\t', 'quad'
			print_file(f"{indent}}}", file = file)
			print_file(f"{indent}else", file = file)
			print_file(f"{indent}{{", file = file)

			if 'w' in sizes:
				# WORD
				print_file(f"{indent}\tswitch(prs->operation_size)", file = file)
				print_file(f"{indent}\t{{", file = file)
				print_file(f"{indent}\tcase SIZE_16BIT:", file = file)
				yield indent + '\t\t', 'word'
				print_file(f"{indent}\t\tbreak;", file = file)

				# LONG
				print_file(f"{indent}\tcase SIZE_32BIT:", file = file)
				assert 'l' in sizes
				yield indent + '\t\t', 'long'
				print_file(f"{indent}\t\tbreak;", file = file)

				print_file(f"{indent}\tdefault:", file = file)
				print_file(f"{indent}\t\tassert(false);", file = file)
				print_file(f"{indent}\t}}", file = file)
			else:
				yield indent + '\t', 'long'
			print_file(f"{indent}}}", file = file)
		else:
			if 'q' in sizes and size_option == 'branch':
				# Intel64 only uses 64-bit for branches
				print_file(f"{indent}if(prs->operation_size == SIZE_64BIT && x86_is_intel64(emu))", file = file)
				print_file(f"{indent}{{", file = file)
				yield indent + '\t', 'quad'
				print_file(f"{indent}}}", file = file)
				print_file(f"{indent}else", file = file)

			# WORD
			print_file(f"{indent}switch(prs->operation_size)", file = file)
			print_file(f"{indent}{{", file = file)
			print_file(f"{indent}case SIZE_16BIT:", file = file)
			if 'w' in sizes:
				yield indent + '\t', 'word'
				print_file(f"{indent}\tbreak;", file = file)

			# LONG
			print_file(f"{indent}case SIZE_32BIT:", file = file)
			if 'q' not in sizes:
				print_file(f"{indent}case SIZE_64BIT:", file = file)
			if 'q' in sizes and size_option in {'branch', 'stack'}:
				print_file(f"{indent}\tif(prs->code_size != SIZE_64BIT)", file = file)
				print_file(f"{indent}\t{{", file = file)
				indent += '\t'
			if 'l' not in sizes:
				print_file(sizes)
			assert 'l' in sizes
			yield indent + '\t', 'long'
			print_file(f"{indent}\tbreak;", file = file)
			if 'q' in sizes and size_option in {'branch', 'stack'}:
				indent = indent[:-1]
				print_file(f"{indent}\t}}", file = file)
				print_file(f"{indent}\t__attribute__((fallthrough));", file = file)

			# QUAD
			if 'q' in sizes:
				print_file(f"{indent}case SIZE_64BIT:", file = file)
				yield indent + '\t', 'quad'
				print_file(f"{indent}\tbreak;", file = file)
			print_file(f"{indent}default:", file = file)
			print_file(f"{indent}\tassert(false);", file = file)
			print_file(f"{indent}}}", file = file)

def all_sizes(opds):
	sizes = set()
	for opd in opds:
		if 'size' in opd:
			sizes.update(opd['size'])
	return sizes

def iter_params(s):
	buff = None
	for c in s:
		if c.isalnum():
			if buff is not None:
				buff += c
		elif c == '$':
			if buff is not None:
				yield buff
			buff = c
		else:
			if buff is not None:
				yield buff
			buff = None
	if buff is not None:
		yield buff

def replace(code, replacements):
	for key, value in sorted(replacements.items(), key = lambda pair: -len(pair[0])):
		while True:
			if key.endswith('='):
				operator = None
				ix0 = code.find(key[:-1])
				while ix0 != -1:
					ix1 = ix0 + len(key[:-1])
					while ix1 < len(code) and code[ix1].isspace():
						ix1 += 1
					if ix1 < len(code) and code[ix1] == '=' and \
					not (ix1 + 1 < len(code) and code[ix1 + 1] == '='):
						ix1 += 1
						break
					elif ix1 + 1 < len(code) and code[ix1] in {'%', '^', '&', '*', '-', '+', '|', '/'} and code[ix1 + 1] == '=':
						operator = code[ix1]
						ix1 += 2
						break
					elif ix1 + 2 < len(code) and code[ix1] in {'<', '>'} and code[ix1] == code[ix1 + 1] and code[ix1 + 2] == '=':
						operator = code[ix1:ix1 + 2]
						ix1 += 3
						break
					ix0 = code.find(key[:-1], ix1)
				else:
					break
				ix2 = code.find(';', ix1)
				if operator is None:
					code = code[:ix0] + replace(value, {'$$': code[ix1 + 1:ix2].strip()}) + code[ix2:]
				else:
					code = code[:ix0] + replace(value, {'$$':
						replacements[key[:-1]] + ' ' + operator + ' (' + code[ix1 + 1:ix2].strip() + ')'}) + code[ix2:]
			else:
				ix0 = code.find(key)
				if ix0 == -1:
					break
				ix1 = ix0 + len(key)
				code = code[:ix0] + value + code[ix1:]
	return code

def gen_code(line, infile, code, *ops, indent = '', **kwds):
	replacements = registers.copy()

	if 'cond' in kwds:
		replacements['$C'] = kwds['cond']

	if 'opsize' in kwds:
		opsize = kwds['opsize']
		opbits = get_bits(SIZE_NAMES[opsize])
		replacements['$O'] = str(opbits)
		opsize2 = {'l': 'd'}.get(opsize, opsize)
		replacements['$ax.$O']  = f'x86_register_get{opbits}(emu, X86_R_AX)'
		replacements['$ax.$O='] = f'x86_register_set{opbits}(emu, X86_R_AX, $$)'
		replacements['$cx.$O']  = f'x86_register_get{opbits}(emu, X86_R_CX)'
		replacements['$cx.$O='] = f'x86_register_set{opbits}(emu, X86_R_CX, $$)'
		replacements['$dx.$O']  = f'x86_register_get{opbits}(emu, X86_R_DX)'
		replacements['$dx.$O='] = f'x86_register_set{opbits}(emu, X86_R_DX, $$)'
		replacements['$bx.$O']  = f'x86_register_get{opbits}(emu, X86_R_BX)'
		replacements['$bx.$O='] = f'x86_register_set{opbits}(emu, X86_R_BX, $$)'
		if opbits != 8:
			replacements['$sp.$O']  = f'x86_register_get{opbits}(emu, X86_R_SP)'
			replacements['$sp.$O='] = f'x86_register_set{opbits}(emu, X86_R_SP, $$)'
			replacements['$bp.$O']  = f'x86_register_get{opbits}(emu, X86_R_BP)'
			replacements['$bp.$O='] = f'x86_register_set{opbits}(emu, X86_R_BP, $$)'
			replacements['$si.$O']  = f'x86_register_get{opbits}(emu, X86_R_SI)'
			replacements['$si.$O='] = f'x86_register_set{opbits}(emu, X86_R_SI, $$)'
			replacements['$di.$O']  = f'x86_register_get{opbits}(emu, X86_R_DI)'
			replacements['$di.$O='] = f'x86_register_set{opbits}(emu, X86_R_DI, $$)'

		replacements['$flags.$O'] = 'x86_flags_get_image' + str(get_bits(SIZE_NAMES[opsize])) + '(emu)'
		replacements['$flags.$O='] = 'x86_flags_set_image' + str(get_bits(SIZE_NAMES[opsize])) + '(emu, $$)'

		if opsize != 'b':
			halfopsize = {'w': 'b', 'l': 'w', 'q': 'l'}[opsize]
			halfopbits = get_bits(SIZE_NAMES[halfopsize])
			replacements['$Ohalf'] = str(halfopbits)
			replacements['$ax.$Ohalf']  = f'x86_register_get{halfopbits}(emu, X86_R_AX)'
			replacements['$ax.$Ohalf='] = f'x86_register_set{halfopbits}(emu, X86_R_AX, $$)'

		dupopsize = {'b': 'w', 'w': 'l', 'l': 'q', 'q': 'o'}[opsize]
		replacements['$Odup'] = str(get_bits(SIZE_NAMES[dupopsize])) if dupopsize != 'o' else '128'

	if 'adsize' in kwds:
		adsize = kwds['adsize']
		adsize2 = {'l': 'd'}.get(adsize, adsize)
		adbits = get_bits(SIZE_NAMES[adsize])
		replacements['$A'] = str(adbits)
		replacements['$cx.$A']  = f'x86_register_get{adbits}(emu, X86_R_CX)'
		replacements['$cx.$A='] = f'x86_register_set{adbits}(emu, X86_R_CX, $$)'
		replacements['$bx.$A']  = f'x86_register_get{adbits}(emu, X86_R_BX)'
		replacements['$bx.$A='] = f'x86_register_set{adbits}(emu, X86_R_BX, $$)'
		replacements['$si.$A']  = f'x86_register_get{adbits}(emu, X86_R_SI)'
		replacements['$si.$A='] = f'x86_register_set{adbits}(emu, X86_R_SI, $$)'
		replacements['$di.$A']  = f'x86_register_get{adbits}(emu, X86_R_DI)'
		replacements['$di.$A='] = f'x86_register_set{adbits}(emu, X86_R_DI, $$)'

	if 'stksize' in kwds:
		stksize = kwds['stksize']
		stksize2 = {'l': 'd'}.get(stksize, stksize)
		stkbits = get_bits(SIZE_NAMES[stksize])
		replacements['$S'] = str(stkbits)
		replacements['$sp.$S']  = f'x86_register_get{stkbits}(emu, X86_R_SP)'
		replacements['$sp.$S='] = f'x86_register_set{stkbits}(emu, X86_R_SP, $$)'
		replacements['$bp.$S']  = f'x86_register_get{stkbits}(emu, X86_R_BP)'
		replacements['$bp.$S='] = f'x86_register_set{stkbits}(emu, X86_R_BP, $$)'

	for i, op in enumerate(ops):
		if 'opsize' in kwds:
			replacements1 = registers.copy()
			replacements1['$?'] = ''
			replacements1['$S'] = str(get_bits(SIZE_NAMES[kwds['opsize']]))
			replacements1['$Sc'] = '(uint32_t)' if kwds['opsize'] == 'l' else '' # cast
			replacements[f'${i}.$O'] = replace(op.get('read', '0/*TODO*/'), replacements1)
			replacements[f'${i}.$O='] = replace(op.get('write', '0/*TODO*/'), replacements1)

			read = op.get('read', '')
			read2 = op.get('read2')
			if read2 is not None or '$?' in read:
				if read2 is not None:
					read = read2
				replacements1['$?'] = ' + ' + str(get_bytes(SIZE_NAMES[kwds['opsize']]))
				replacements[f'${i}@$O.$O'] = replace(read, replacements1)
				replacements1['$S'] = '16'
				replacements1['$Sc'] = '(uint32_t)' if kwds['opsize'] == 'l' else '' # cast
				replacements[f'${i}@$O.w'] = replace(read, replacements1)
				replacements1['$?'] = ' + 2'
				replacements1['$S'] = str(get_bits(SIZE_NAMES[kwds['opsize']]))
				replacements[f'${i}@w.$O'] = replace(read, replacements1)

			write = op.get('write', '')
			write2 = op.get('write2')
			if write2 is not None or '$?' in write:
				if write2 is not None:
					write = write2
				replacements1['$?'] = ' + ' + str(get_bytes(SIZE_NAMES[kwds['opsize']]))
				replacements1['$S'] = str(get_bits(SIZE_NAMES[kwds['opsize']]))
				replacements1['$Sc'] = '(uint32_t)' if kwds['opsize'] == 'l' else '' # cast
				replacements[f'${i}@$O.$O='] = replace(write, replacements1)

				replacements1['$?'] = ' + 2'
				replacements[f'${i}@w.$O='] = replace(write, replacements1)

		read = op.get('read', '')
		read2 = op.get('read2')
		if read2 is not None or '$?' in read:
			if read2 is not None:
				read = read2
			replacements1 = registers.copy()
			replacements1['$?'] = ' + 2'
			replacements1['$S'] = '32'
			replacements1['$Sc'] = '(uint32_t)' # cast
			replacements[f'${i}@w.l'] = replace(read, replacements1)

		for opdsize in 'bwlq':
			replacements1 = registers.copy()
			replacements1['$?'] = ''
			replacements1['$S'] = str(get_bits(SIZE_NAMES[opdsize]))
			replacements1['$Sc'] = '(uint32_t)' if opdsize == 'l' else '' # cast
			replacements[f'${i}.{opdsize}'] = replace(op.get('read', '0/*TODO*/'), replacements1)
			replacements[f'${i}.{opdsize}='] = replace(op.get('write', '0/*TODO*/'), replacements1)

			read = op.get('read', '')
			read2 = op.get('read2')
			if read2 is not None or '$?' in read:
				if read2 is not None:
					read = read2
				replacements1['$?'] = ' + ' + str(get_bytes(SIZE_NAMES[opdsize]))
				replacements1['$S'] = '16'
				replacements1['$Sc'] = ''
				replacements[f'${i}@{opdsize}.w'] = replace(read, replacements1)

		# ??? for now, it is needed for immediate parameters
		replacements1 = registers.copy()
		replacements1['$?'] = ''
		replacements[f'${i}'] = replace(op.get('read', '0/*TODO*/'), replacements1)
		replacements[f'${i}='] = replace(op.get('write', '0/*TODO*/'), replacements1)

		if 'size' in op and len(op['size']) == 1:
			replacements[f'${i}.size'] = str(get_bits(SIZE_NAMES[op['size']]))
	next_line = file_line + code.count('\n') + 3
	return f"#line {line} \"{infile}\"\n" + indent + replace(code, replacements).replace('\n', '\n' + indent) + f"\n#line {next_line} \"{outfile}\""

def gen_code80(line_infile_code, *ops, indent = '', **kwds):
	line, infile, code = line_infile_code
	replacements = registers80.copy()

	replacements['$dsp'] = '0'
	for op in ops:
		if 'prepare' in op:
			# hacky way to figure out if there is an immediate/displacement stored in an operand
			# this is needed for th CB instructions
			replacements['$dsp'] = 'imm0'
			break

	for i, op in enumerate(ops):
		replacements[f'${i}'] = replace(op.get('read', '/*TODO*/'), registers80)
		replacements[f'${i}='] = replace(op.get('write', '/*TODO*/'), registers80)
		if 'condition' in op:
			replacements[f'${i}.cond'] = op.get('condition')

	next_line = file_line + code.count('\n') + 3
	return f"#line {line} \"{infile}\"\n" + indent + replace(code, replacements).replace('\n', '\n' + indent) + f"\n#line {next_line} \"{outfile}\""

registers = {
	'$s': 'emu->sr',

	'$al': 'x86_register_get8_low(emu, X86_R_AX)',
	'$al=': 'x86_register_set8_low(emu, X86_R_AX, $$)',
	'$ah': 'x86_register_get8_high(emu, X86_R_AX)',
	'$ah=': 'x86_register_set8_high(emu, X86_R_AX, $$)',
	'$ax': 'x86_register_get16(emu, X86_R_AX)',
	'$ax=': 'x86_register_set16(emu, X86_R_AX, $$)',
	'$eax': 'x86_register_get32(emu, X86_R_AX)',
	'$eax=': 'x86_register_set32(emu, X86_R_AX, $$)',
	'$rax': 'x86_register_get64(emu, X86_R_AX)',
	'$rax=': 'x86_register_set64(emu, X86_R_AX, $$)',

	'$cl': 'x86_register_get8_low(emu, X86_R_CX)',
	'$cl=': 'x86_register_set8_low(emu, X86_R_CX, $$)',
	'$ch': 'x86_register_get8_high(emu, X86_R_CX)',
	'$ch=': 'x86_register_set8_high(emu, X86_R_CX, $$)',
	'$cx': 'x86_register_get16(emu, X86_R_CX)',
	'$cx=': 'x86_register_set16(emu, X86_R_CX, $$)',
	'$ecx': 'x86_register_get32(emu, X86_R_CX)',
	'$ecx=': 'x86_register_set32(emu, X86_R_CX, $$)',
	'$rcx': 'x86_register_get64(emu, X86_R_CX)',
	'$rcx=': 'x86_register_set64(emu, X86_R_CX, $$)',

	'$dl': 'x86_register_get8_low(emu, X86_R_DX)',
	'$dl=': 'x86_register_set8_low(emu, X86_R_DX, $$)',
	'$dh': 'x86_register_get8_high(emu, X86_R_DX)',
	'$dh=': 'x86_register_set8_high(emu, X86_R_DX, $$)',
	'$dx': 'x86_register_get16(emu, X86_R_DX)',
	'$dx=': 'x86_register_set16(emu, X86_R_DX, $$)',
	'$edx': 'x86_register_get32(emu, X86_R_DX)',
	'$edx=': 'x86_register_set32(emu, X86_R_DX, $$)',
	'$rdx': 'x86_register_get64(emu, X86_R_DX)',
	'$rdx=': 'x86_register_set64(emu, X86_R_DX, $$)',

	'$bl': 'x86_register_get8_low(emu, X86_R_BX)',
	'$bl=': 'x86_register_set8_low(emu, X86_R_BX, $$)',
	'$bh': 'x86_register_get8_high(emu, X86_R_BX)',
	'$bh=': 'x86_register_set8_high(emu, X86_R_BX, $$)',
	'$bx': 'x86_register_get16(emu, X86_R_BX)',
	'$bx=': 'x86_register_set16(emu, X86_R_BX, $$)',
	'$ebx': 'x86_register_get32(emu, X86_R_BX)',
	'$ebx=': 'x86_register_set32(emu, X86_R_BX, $$)',
	'$rbx': 'x86_register_get64(emu, X86_R_BX)',
	'$rbx=': 'x86_register_set64(emu, X86_R_BX, $$)',

	'$spl': 'x86_register_get8_low(emu, X86_R_SP)',
	'$spl=': 'x86_register_set8_low(emu, X86_R_SP, $$)',
	'$sp': 'x86_register_get16(emu, X86_R_SP)',
	'$sp=': 'x86_register_set16(emu, X86_R_SP, $$)',
	'$esp': 'x86_register_get32(emu, X86_R_SP)',
	'$esp=': 'x86_register_set32(emu, X86_R_SP, $$)',
	'$rsp': 'x86_register_get64(emu, X86_R_SP)',
	'$rsp=': 'x86_register_set64(emu, X86_R_SP, $$)',

	'$bpl': 'x86_register_get8_low(emu, X86_R_BP)',
	'$bpl=': 'x86_register_set8_low(emu, X86_R_BP, $$)',
	'$bp': 'x86_register_get16(emu, X86_R_BP)',
	'$bp=': 'x86_register_set16(emu, X86_R_BP, $$)',
	'$ebp': 'x86_register_get32(emu, X86_R_BP)',
	'$ebp=': 'x86_register_set32(emu, X86_R_BP, $$)',
	'$rbp': 'x86_register_get64(emu, X86_R_BP)',
	'$rbp=': 'x86_register_set64(emu, X86_R_BP, $$)',

	'$sil': 'x86_register_get8_low(emu, X86_R_SI)',
	'$sil=': 'x86_register_set8_low(emu, X86_R_SI, $$)',
	'$si': 'x86_register_get16(emu, X86_R_SI)',
	'$si=': 'x86_register_set16(emu, X86_R_SI, $$)',
	'$esi': 'x86_register_get32(emu, X86_R_SI)',
	'$esi=': 'x86_register_set32(emu, X86_R_SI, $$)',
	'$rsi': 'x86_register_get64(emu, X86_R_SI)',
	'$rsi=': 'x86_register_set64(emu, X86_R_SI, $$)',

	'$dil': 'x86_register_get8_low(emu, X86_R_DI)',
	'$dil=': 'x86_register_set8_low(emu, X86_R_DI, $$)',
	'$di': 'x86_register_get16(emu, X86_R_DI)',
	'$di=': 'x86_register_set16(emu, X86_R_DI, $$)',
	'$edi': 'x86_register_get32(emu, X86_R_DI)',
	'$edi=': 'x86_register_set32(emu, X86_R_DI, $$)',
	'$rdi': 'x86_register_get64(emu, X86_R_DI)',
	'$rdi=': 'x86_register_set64(emu, X86_R_DI, $$)',

	'$flagsl': 'x86_flags_get_image8(emu)',
	'$flagsl=': 'x86_flags_set_image8(emu, $$)',
	'$flags': 'x86_flags_get_image16(emu)',
	'$flags=': 'x86_flags_set_image16(emu, $$)',

	'$ip': '(emu->xip & 0xFFFF)',
	'$ip=': 'x86_jump(emu, ($$) & 0xFFFF)',
	'$eip': '(emu->xip & 0xFFFFFFFF)',
	'$eip=': 'x86_jump(emu, ($$) & 0xFFFFFFFF)',
	'$rip': 'emu->xip',
	'$rip=': 'x86_jump(emu, $$)',
	'$old_rip': 'emu->old_xip',
	'$cf': 'emu->cf',
	'$cf=': 'emu->cf = ($$) != 0 ? X86_FL_CF : 0',
	'$pf': 'emu->pf',
	'$pf=': 'emu->pf = ($$) != 0 ? X86_FL_PF : 0',
	'$af': 'emu->af',
	'$af=': 'emu->af = ($$) != 0 ? X86_FL_AF : 0',
	'$zf': 'emu->zf',
	'$zf=': 'emu->zf = ($$) != 0 ? X86_FL_ZF : 0',
	'$sf': 'emu->sf',
	'$sf=': 'emu->sf = ($$) != 0 ? X86_FL_SF : 0',
	'$tf': 'emu->tf',
	'$tf=': 'emu->af = ($$) != 0 ? X86_FL_TF : 0',
	'$if': 'emu->_if',
	'$if=': 'emu->_if = ($$) != 0 ? X86_FL_IF : 0',
	'$df': 'emu->df',
	'$df=': 'emu->df = ($$) != 0 ? X86_FL_DF : 0',
	'$of': 'emu->of',
	'$of=': 'emu->of = ($$) != 0 ? X86_FL_OF : 0',
	'$ac': 'emu->ac',
	'$ac=': 'emu->ac = ($$) != 0 ? X86_FL_AC : 0',
	'$cs': 'emu->sr[X86_R_CS].selector',
	'$ds': 'emu->sr[X86_R_DS].selector',
	'$ds=': 'x86_segment_set(emu, X86_R_DS, $$)',
	'$es': 'emu->sr[X86_R_ES].selector',
	'$es=': 'x86_segment_set(emu, X86_R_ES, $$)',
	'$ss': 'emu->sr[X86_R_SS].selector',
	'$ss=': 'x86_segment_set(emu, X86_R_SS, $$)',
	'$fs': 'emu->sr[X86_R_FS].selector',
	'$fs=': 'x86_segment_set(emu, X86_R_FS, $$)',
	'$gs': 'emu->sr[X86_R_GS].selector',
	'$gs=': 'x86_segment_set(emu, X86_R_GS, $$)',
	'$ds2': 'emu->sr[X86_R_DS2].selector',
	'$ds2=': 'x86_segment_set(emu, X86_R_DS2, $$)',
	'$ds3': 'emu->sr[X86_R_DS3].selector',
	'$ds3=': 'x86_segment_set(emu, X86_R_DS3, $$)',
	'$cpl': 'x86_get_cpl(emu)',
	'$cpl=': 'x86_set_cpl(emu, $$)',
	'$ldtr': 'emu->sr[X86_R_LDTR].selector',
	'$tr': 'emu->sr[X86_R_TR].selector',
	'$0.off': '_off', # Note: this only works if $0 is the EA
	'$1.off': '_off', # Note: this only works if $1 is the EA

	'$bank': 'emu->bank[emu->rb]',

	'$st': 'x87_register_get80(emu, 0)',
	'$st=': 'x87_register_set80(emu, 0, $$)',
	'$c0': '(emu->x87.sw & X87_SW_C0)',
	'$c0=': '($$) != 0 ? (emu->x87.sw |= X87_SW_C0) : (emu->x87.sw &= ~X87_SW_C0)',
}

for i in range(8):
	registers[f'$st{i}'] = f'x87_register_get80(emu, {i})'
	registers[f'$st{i}='] = f'x87_register_set80(emu, {i}, $$)'

registers80 = {
	'$f': 'x86_get_low(emu->bank[emu->af_bank].af)',
	'$f=': 'x86_set_low(&emu->bank[emu->af_bank].af, $$)',
	'$cf': '(emu->bank[emu->af_bank].af & X86_FL_CF)',
	'$cf=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= X86_FL_CF) : (emu->bank[emu->af_bank].af &= ~X86_FL_CF)',
	'$nf': '(emu->bank[emu->af_bank].af & 0x0002)',
	'$nf=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= 0x0002) : (emu->bank[emu->af_bank].af &= ~0x0002)',
	'$pf': '(emu->bank[emu->af_bank].af & X86_FL_PF)',
	'$pf=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= X86_FL_PF) : (emu->bank[emu->af_bank].af &= ~X86_FL_PF)',
	'$af': '(emu->bank[emu->af_bank].af & X86_FL_AF)',
	'$af=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= X86_FL_AF) : (emu->bank[emu->af_bank].af &= ~X86_FL_AF)',
	'$zf': '(emu->bank[emu->af_bank].af & X86_FL_ZF)',
	'$zf=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= X86_FL_ZF) : (emu->bank[emu->af_bank].af &= ~X86_FL_ZF)',
	'$sf': '(emu->bank[emu->af_bank].af & X86_FL_SF)',
	'$sf=': '($$) != 0 ? (emu->bank[emu->af_bank].af |= X86_FL_SF) : (emu->bank[emu->af_bank].af &= ~X86_FL_SF)',
	'$if': 'emu->iff1',
	'$b': 'x86_get_high(emu->bank[emu->main_bank].bc)',
	'$b=': 'x86_set_high(&emu->bank[emu->main_bank].bc, $$)',
	'$a': 'x86_get_high(emu->bank[emu->af_bank].af)',
	'$a=': 'x86_set_high(&emu->bank[emu->af_bank].af, $$)',
	'$bc': 'emu->bank[emu->main_bank].bc',
	'$de': 'emu->bank[emu->main_bank].de',
	'$hl': 'emu->bank[emu->main_bank].hl',
	'$pc': 'emu->pc',
	'$pc=': 'emu->pc = $$',
	'$old_pc': 'old_pc',
}

MISSING = set()
def print_instruction(path, indent, actual_range, entry, discriminator, index, file, mode, modrm):
	global INSTRUCTIONS
	global MISSING
	assert type(entry) is Instruction

	if modrm is None or entry.kwds.get('modrm') == False:
		print_file(f"{indent}/* {entry.kwds['mnem']}{' ' + ', '.join(entry.kwds['opds']) if len(entry.kwds['opds']) > 0 else ''} */", file = file)

	if modrm is None and 'modrm' in entry.kwds:
		assert False

	if entry.kwds['mnem'] in {'ES:', 'CS:', 'SS:', 'DS:', 'FS:', 'GS:', 'DS2:', 'DS3:', 'IRAM:'}:
		segreg = entry.kwds['mnem'][:-1]
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->segment = X86_R_{segreg};", file = file)
		if segreg in {'DS3', 'IRAM'}:
			print_file(f"{indent}prs->destination_segment = prs->segment;", file = file)
		else:
			print_file(f"{indent}prs->source_segment = prs->segment;", file = file)
		if segreg == 'DS2':
			print_file(f"{indent}prs->source_segment2 = prs->segment;", file = file)
		if segreg != 'DS3':
			print_file(f"{indent}prs->source_segment3 = prs->segment;", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'REX:':
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->rex_prefix = true;", file = file)
		print_file(f"{indent}prs->rex_w = (opcode & X86_REX_W) != 0;", file = file)
		print_file(f"{indent}prs->rex_r = opcode & X86_REX_R ? 8 : 0;", file = file)
		print_file(f"{indent}prs->evex_vx = prs->rex_x = opcode & X86_REX_X ? 8 : 0;", file = file)
		print_file(f"{indent}prs->evex_vb = prs->rex_b = opcode & X86_REX_B ? 8 : 0;", file = file)
		print_file(f"{indent}if(prs->rex_w)", file = file)
		print_file(f"{indent}goto restart; // end of legacy codes", file = file)
		return False
	elif entry.kwds['mnem'] == 'OPSIZE:':
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->operation_size = prs->code_size == SIZE_16BIT ? SIZE_32BIT : SIZE_16BIT;", file = file)
		print_file(f"{indent}if(prs->simd_prefix == X86_PREF_NONE)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tprs->simd_prefix = X86_PREF_66;", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'ADSIZE:':
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->address_size = prs->code_size == SIZE_32BIT ? SIZE_16BIT : SIZE_32BIT;", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'LOCK:':
		print_file(f"{indent}if(x86_is_ia64(emu))", file = file)
		print_file(f"{indent}\tx86_ia64_intercept(emu, 0); // TODO", file = file) # TODO
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->lock_prefix = true;", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'USR:':
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->user_mode = true;", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] in {'REPZ:', 'REPNZ:', 'REPC:', 'REPNC:'}:
		prefix = entry.kwds['mnem'][:-1]
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}\tUNDEFINED(); // TODO: illegal instruction", file = file)
		print_file(f"{indent}}}", file = file)
		print_file(f"{indent}prs->rep_prefix = X86_PREF_{prefix};", file = file)
		if prefix in {'REPZ', 'REPNZ'}:
			code = {'REPNZ': 'F2', 'REPZ': 'F3'}[prefix]
			print_file(f"{indent}prs->simd_prefix = X86_PREF_{code};", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'VEX2:':
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}if(prs->simd_prefix != X86_PREF_NONE || prs->rex_prefix)", file = file)
		print_file(f"{indent}\tUNDEFINED();", file = file)
		if modrm is None:
			print_file(f"{indent}\tuint8_t byte = x86_fetch8(prs, emu);", file = file)
		else:
			print_file(f"{indent}\tuint8_t byte = prs->modrm_byte;", file = file)
		print_file(f"{indent}\tprs->rex_r = (byte & 0x80) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vx = prs->rex_x = 0;", file = file)
		print_file(f"{indent}\tprs->evex_vb = prs->rex_b = 0;", file = file)
		print_file(f"{indent}\tprs->opcode_map = 1;", file = file)
		print_file(f"{indent}\tprs->rex_w = false;", file = file)
		print_file(f"{indent}\tprs->vex_v = ((byte >> 3) & 0xF) ^ 0xF;", file = file)
		print_file(f"{indent}\tprs->vex_l = (byte & 0x04) != 0 ? 1 : 0;", file = file)
		print_file(f"{indent}\tprs->simd_prefix = (x86_simd_prefix_t)(byte & 3);", file = file)
		print_file(f"{indent}}}", file = file)
		# TODO: use opcode_map
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] in {'VEX3:', 'XOP:'}:
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}if(prs->simd_prefix != X86_PREF_NONE || prs->rex_prefix)", file = file)
		print_file(f"{indent}\tUNDEFINED();", file = file)
		if modrm is None:
			print_file(f"{indent}\tuint8_t byte = x86_fetch8(prs, emu);", file = file)
		else:
			print_file(f"{indent}\tuint8_t byte = prs->modrm_byte;", file = file)
		print_file(f"{indent}\tprs->rex_r = (byte & 0x80) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vx = prs->rex_x = (byte & 0x40) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vb = prs->rex_b = (byte & 0x20) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->opcode_map = byte & 0x1F;", file = file)
		if entry.kwds['mnem'] == 'VEX3':
			print_file(f"{indent}\tif(prs->opcode_map != 0x01 && prs->opcode_map != 0x02 && prs->opcode_map != 0x03)", file = file)
			print_file(f"{indent}\t\tUNDEFINED();", file = file)
		elif entry.kwds['mnem'] == 'XOP:':
			print_file(f"{indent}\t// TODO: check whether 0x0A is valid for this CPU", file = file)
			print_file(f"{indent}\tif(prs->opcode_map != 0x08 && prs->opcode_map != 0x09 && prs->opcode_map != 0x0A)", file = file)
			print_file(f"{indent}\t\tUNDEFINED();", file = file)
		print_file(f"{indent}\tbyte = x86_fetch8(prs, emu);", file = file)
		print_file(f"{indent}\tprs->rex_w = (byte & 0x80) != 0;", file = file)
		print_file(f"{indent}\tprs->vex_v = ((byte >> 3) & 0xF) ^ 0xF;", file = file)
		print_file(f"{indent}\tprs->vex_l = (byte & 0x04) != 0 ? 1 : 0;", file = file)
		print_file(f"{indent}\tprs->simd_prefix = (x86_simd_prefix_t)(byte & 3);", file = file)
		print_file(f"{indent}}}", file = file)
		# TODO: use opcode_map
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] in {'EVEX:', 'MVEX:'}:
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}if(prs->simd_prefix != X86_PREF_NONE || prs->rex_prefix)", file = file)
		print_file(f"{indent}\tUNDEFINED();", file = file)
		if modrm is None:
			print_file(f"{indent}\tuint8_t byte = x86_fetch8(prs, emu);", file = file)
		else:
			print_file(f"{indent}\tuint8_t byte = prs->modrm_byte;", file = file)
		print_file(f"{indent}\tprs->rex_r = (byte & 0x80) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vx = prs->rex_x = (byte & 0x40) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vb = prs->rex_b = (byte & 0x20) == 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vb += (byte & 0x40) == 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_r += (byte & 0x10) == 0 ? 16 : 0;", file = file)
		if entry.kwds['mnem'] == 'EVEX:':
			print_file(f"{indent}\t// TODO: only for extended EVEX", file = file)
			# TODO: otherwise assert it is 0
			print_file(f"{indent}\tprs->rex_b += (byte & 0x08) != 0 ? 16 : 0;", file = file)
			print_file(f"{indent}\tprs->opcode_map = byte & 0x07;", file = file)
			print_file(f"{indent}\tif(prs->opcode_map != 0x01 && prs->opcode_map != 0x02 && prs->opcode_map != 0x03 && prs->opcode_map != 0x04)", file = file)
			print_file(f"{indent}\t\tUNDEFINED();", file = file)
		elif entry.kwds['mnem'] == 'MVEX:':
			print_file(f"{indent}\tprs->opcode_map = byte & 0x0F;", file = file)
			print_file(f"{indent}\tif(prs->opcode_map != 0x01 && prs->opcode_map != 0x02 && prs->opcode_map != 0x03)", file = file)
			print_file(f"{indent}\t\tUNDEFINED();", file = file)
		print_file(f"{indent}\tbyte = x86_fetch8(prs, emu);", file = file)
		print_file(f"{indent}\tprs->rex_w = (byte & 0x80) != 0;", file = file)
		print_file(f"{indent}\tprs->vex_v = ((byte >> 3) & 0xF) ^ 0xF;", file = file)
		if entry.kwds['mnem'] == 'EVEX:':
			print_file(f"{indent}\t// TODO: only for extended EVEX", file = file)
			print_file(f"{indent}\tprs->rex_x += (byte & 0x04) == 0 ? 16 : 0;", file = file)
			# TODO: otherwise assert it is 1
		elif entry.kwds['mnem'] == 'MVEX:':
			print_file(f"{indent}\tif((byte & 0x04) != 0)", file = file)
			print_file(f"{indent}\t\tUNDEFINED(); // TODO: what is the right exception?", file = file)
		print_file(f"{indent}\tprs->simd_prefix = (x86_simd_prefix_t)(byte & 3);", file = file)
		print_file(f"{indent}\tbyte = x86_fetch8(prs, emu);", file = file)
		print_file(f"{indent}\tprs->evex_z = (byte & 0x80) != 0;", file = file)
		if entry.kwds['mnem'] == 'EVEX:':
			print_file(f"{indent}\tprs->vex_l = (byte >> 5) & 3;", file = file)
			print_file(f"{indent}\tprs->rex_b = (byte & 0x10) != 0;", file = file)
		else:
			pass # TODO: 3 bits swizzle/etc. control
		print_file(f"{indent}\tprs->vex_v += (byte & 0x08) == 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_vx += (byte & 0x08) == 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->evex_a = byte & 0x0F;", file = file)
		print_file(f"{indent}}}", file = file)
		# TODO: use opcode_map
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'REX2:':
		print_file(f"{indent}{{", file = file)
		print_file(f"{indent}if(prs->rex_prefix)", file = file)
		print_file(f"{indent}\tUNDEFINED();", file = file)
		print_file(f"{indent}\tuint8_t byte = x86_fetch8(prs, emu);", file = file)
		print_file(f"{indent}\tprs->opcode_map = (byte & 0x80) != 0 ? 1 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_r = (byte & 0x40) != 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_x = (byte & 0x20) != 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_b = (byte & 0x10) != 0 ? 16 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_w = (byte & 0x08) != 0;", file = file)
		print_file(f"{indent}\tprs->rex_r += (byte & 0x04) != 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_x += (byte & 0x02) != 0 ? 8 : 0;", file = file)
		print_file(f"{indent}\tprs->rex_b += (byte & 0x01) != 0 ? 8 : 0;", file = file)
		print_file(f"{indent}}}", file = file)
		# TODO: use opcode_map
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'] == 'L10M62:' or entry.kwds['mnem'] == 'L10MD6:':
		print_file(f"{indent}/* TODO */;", file = file)
		print_file(f"{indent}goto restart;", file = file)
		return False
	elif entry.kwds['mnem'].endswith(':'):
		print(entry.kwds['mnem'])
		assert False
	elif mode == '8':
		opds = []
		for opd in entry.kwds['opds']:
			if opd not in X80_OPERAND_CODE:
				print("Unknown operand:", repr(opd))
				MISSING.add(entry.kwds['mnem']) # store operands for further study
				print_file(f"{indent}/* TODO */;", file = file)
				return True
			opd = X80_OPERAND_CODE.get(opd, {})
			opds.append(opd)

		indent1 = indent

		cnt = 0
		for i in range(len(opds)):
			opd = opds[i]
			if 'prepare' in opd:
				opd = recursive_replace(opd, '$#', str(cnt))
				if cnt == 0:
					print_file(indent1 + '{', file = file)
					indent1 += '\t'
				cnt += 1
				print_file(indent1 + opd['prepare'].replace('\n', '\n' + indent1), file = file)
				#print_file(f"{indent1}(void)imm{cnt - 1};", file = file)

				opds[i] = opd

		syntaxes = ['intel', '']
		if 'intel' not in entry.kwds:
			syntaxes.remove('intel')

		for syntax in syntaxes:
			if syntax == '':
				_mnem = entry.kwds['mnem'].lower()
				_opds = opds
			else:
				_mnem = entry.kwds[syntax][0].lower()
				_opds = []

				i = 0
				for opd in entry.kwds[syntax][1]:
					opd = X80_OPERAND_CODE.get(opd, {})
					if 'prepare' in opd:
						opd = recursive_replace(opd, '$#', str(i))
						i += 1
					_opds.append(opd)

			pars = ""
			args = ""
			for opd in _opds:
				if 'format' not in opd:
					continue # TODO
				fmt = opd['format']
				if type(fmt) is dict:
					fmt = fmt[mode]
				if pars != "":
					pars += ", "
				pars += fmt[0]
				for arg in fmt[1]:
					args += ", " + arg

			if pars == "":
				assert args == ""
				line = f'DEBUG("{_mnem}\\n");'
			else:
				line = f'DEBUG("{_mnem}\\t{pars}\\n"{args});'

			if len(syntaxes) == 1:
				print_file(f'{indent1}{line}', file = file)
			else:
				if syntax == 'intel':
					print_file(f'{indent1}if(prs->use_intel8080_syntax)', file = file)
				else:
					print_file(f'{indent1}else', file = file)
				print_file(f'{indent1}{{', file = file)
				print_file(f'{indent1}\t{line}', file = file)
				print_file(f'{indent1}}}', file = file)

		mnem = entry.kwds['mnem']

		if 'Z80.' + mnem in INSTRUCTIONS:
			for parts, code in INSTRUCTIONS['Z80.' + mnem]:
				if parts != []:
					condition = False
					for part in parts:
						clause = True
						for morcel in part.split(' '):
							if morcel.startswith('op='):
								if morcel[3] == 'b':
									if entry.kwds['opds'][0].lower() not in {'a', 'b', 'c', 'd', 'e', 'h', 'l', '(hl)', 'ixh', 'ixl', '(ix+ib)', 'iyh', 'iyl', '(iy+ib)', 'i', 'r'}:
										clause = False
										break
								elif morcel[3] == 'w':
									if entry.kwds['opds'][0].lower() not in {'bc', 'de', 'hl', 'sp', 'ix', 'iy'}:
										clause = False
										break
							elif morcel.startswith('cnt='):
								if len(opds) != int(morcel[4:]):
									clause = False
									break
							elif morcel.startswith('op0='):
								if entry.kwds['opds'][0].lower() != morcel[4:]:
									clause = False
									break
							elif morcel.startswith('op1='):
								if entry.kwds['opds'][1].lower() != morcel[4:]:
									clause = False
									break
							else:
								print_file("// PARTS:", parts, file = file)
								clause = False
								break
						if clause:
							condition = True
							break
					if not condition:
						continue
				kwds = {}

				print_file(f"{indent1}if(!execute)", file = file)
				print_file(f"{indent1}\treturn X86_RESULT(X86_RESULT_SUCCESS, 0);", file = file)

				print_file(f"{indent1}{{", file = file)
				print_file(gen_code80(code, *opds, indent = indent1 + '\t', **kwds), file = file)
				print_file(f"{indent1}}}", file = file)

				break # found pattern, no need to continue searching
			else:
				print_file("// MISSING CONDITIONS:", entry.kwds['mnem'], file = file)
		else:
			print_file("// MISSING INSTRUCTION:", entry.kwds['mnem'], file = file)

		if cnt > 0:
			print_file(indent1[:-1] + '}', file = file)

		return True
	else:
		opds = []
		for opd in entry.kwds['opds']:
			if mode != '87':
				opd = OPERAND_CODE.get(opd, {})
			else:
				opd = X87_OPERAND_CODE.get(opd, {})
			if modrm in opd:
				opd = opd[modrm]
			opds.append(opd)

		sizes = all_sizes(opds)
		size_option = None

		if entry.kwds['mnem'][-1] == 'b':
			sizes = 'b'
		elif entry.kwds['mnem'][-1] == 'z' or entry.kwds['mnem'] in {'POPAd', 'PUSHAd'}:
			sizes = 'wl'
		elif entry.kwds['mnem'][-1] == 'v' or entry.kwds['mnem'] in {'IRET', 'RETF'}:
			sizes = 'wlq'
		elif entry.kwds['mnem'] in {'CBW/CWDE/CDQE', 'CWD/CDQ/CQO'}:
			sizes = 'wlq'
		elif entry.kwds['mnem'] == 'MOV' and len(entry.kwds['opds']) == 3 and entry.kwds['opds'][0] in {'DS2', 'DS3'}:
			# only 16-bit
			sizes = 'w'
		elif entry.kwds['mnem'] in {'CALL', 'JMP', 'RET'} or entry.kwds['mnem'].startswith('LOOP') or entry.kwds['mnem'].startswith('J'):
			sizes = 'wlq'
			size_option = 'branch'
		elif entry.kwds['mnem'] in {'POP', 'PUSH', 'POPF', 'PUSHF', 'ENTER', 'LEAVE'}:
			sizes = 'wlq'
			size_option = 'stack'
		elif entry.kwds['mnem'] in {'SGDT', 'SIDT'}:
			sizes = 'lq'
			size_option = 'control'
		elif entry.kwds['mnem'] in {'LGDT', 'LIDT'} or 'Cy' in entry.kwds['opds'] or 'Dy' in entry.kwds['opds'] or 'Ty' in entry.kwds['opds']:
			size_option = 'control'

		if sizes == set():
			sizes = 'v'

		if len(sizes) > 1 and 'b' in sizes:
			sizes.remove('b')

		if entry.kwds['mnem'] == 'MOV' and ('Cy' in entry.kwds['opds'] or 'Dy' in entry.kwds['opds']):
			print_file(f"{indent}if(!x86_traits_is_long_mode_supported(&prs->cpu_traits) && prs->lock_prefix)", file = file)
			print_file(f"{indent}{{", file = file)
			print_file(f"{indent}\tUNDEFINED();", file = file)
			print_file(f"{indent}}}", file = file)

		for indent1, size in iterate_sizes(sizes, indent, file, size_option = size_option):
			cnt = 0
			for i in range(len(opds)):
				opd = opds[i]
				if 'prepare' in opd:
					opd = recursive_replace(opd, '$#', str(cnt))
					if cnt == 0:
						print_file(indent1 + '{', file = file)
						indent1 += '\t'
					cnt += 1
					print_file(indent1 + replace_patterns(opd['prepare'], size, opd['size']).replace('\n', '\n' + indent1), file = file)
					opds[i] = opd

			mnem = entry.kwds["mnem"]
			if mnem.endswith('b'):
				mnem = mnem[:-1] + 'B'
			elif mnem.endswith('z'):
				mnem = mnem[:-1] + {'word': 'W', 'long': 'D', 'quad': 'D'}[size]
			elif mnem.endswith('v'):
				mnem = mnem[:-1] + {'word': 'W', 'long': 'D', 'quad': 'Q'}[size]
			elif entry.kwds['mnem'] in {'POPAd', 'PUSHAd'}:
				mnem = mnem[:-1] + {'word': 'W', 'long': 'D', 'quad': 'D'}[size]
			elif entry.kwds['mnem'] in {'CBW/CWDE/CDQE', 'CWD/CDQ/CQO'}:
				mnem = mnem.split('/')[['word', 'long', 'quad'].index(size)]
			mnem = mnem.lower()

			syntaxes = ['nec', '']
			if 'nec' not in entry.kwds:
				syntaxes.remove('nec')
			elif size in {'long', 'quad'}:
				# NEC CPUs do not use 32-bit or 64-bit operands
				syntaxes.remove('nec')

			for syntax in syntaxes:
				if syntax == '':
					_mnem = mnem
					_opds = opds
				else:
					_mnem = entry.kwds[syntax][0].lower()
					_opds = []
					i = 0
					for opd in entry.kwds[syntax][1]:
						if mode != '87':
							opd = OPERAND_CODE.get(opd, {})
						else:
							opd = X87_OPERAND_CODE.get(opd, {})
						if modrm in opd:
							opd = opd[modrm]
						if 'prepare' in opd:
							opd = recursive_replace(opd, '$#', str(i))
							i += 1
						_opds.append(opd)

				pars = ""
				args = ""
				for opd in _opds:
					if 'format' not in opd:
						continue # TODO
					fmt = opd['format']
					if type(fmt) is dict:
						fmt = fmt[mode]
					if pars != "":
						pars += ", "
					pars += replace_patterns(fmt[0], size, opd['size'])
					for arg in fmt[1]:
						#assert type(arg) is not dict
						args += ", " + replace_patterns(arg, size, opd['size'])

				if pars == "":
					assert args == ""
					line = f'DEBUG("{_mnem}\\n");'
				else:
					line = f'DEBUG("{_mnem}\\t{pars}\\n"{args});'

				if mode == '87':
					x87_async = mnem in {'fnclex', 'fndisi', 'fneni', 'fninit', 'fnsave', 'fnstcw', 'fnstenv', 'fnstsw', 'fstsg'}

				if mode == '87' and x87_async:
					print_file(f'{indent1}if(!(prs->fpu_type == X87_FPU_INTEGRATED || !sync || !execute))', file = file)
					print_file(f'{indent1}\tDEBUG("[FPU] ~\\t");', file = file)
				elif mode == '87' and not x87_async:
					print_file(f'{indent1}if(prs->fpu_type == X87_FPU_INTEGRATED || !sync || !execute)', file = file)
					print_file(f'{indent1}{{', file = file)
					indent1 += '\t'
				elif _mnem == 'esc':
					print_file(f'{indent1}if(prs->fpu_type != X87_FPU_INTEGRATED && (execute || prs->fpu_type == X87_FPU_NONE))', file = file)
					print_file(f'{indent1}{{', file = file)
					indent1 += '\t'

				if len(syntaxes) == 1:
					print_file(f'{indent1}{line}', file = file)
				else:
					if syntax == 'nec':
						print_file(f'{indent1}if(prs->use_nec_syntax)', file = file)
					else:
						print_file(f'{indent1}else', file = file)
					print_file(f'{indent1}{{', file = file)
					print_file(f'{indent1}\t{line}', file = file)
					print_file(f'{indent1}}}', file = file)

				if mode == '87' and not x87_async:
					indent1 = indent1[:-1]
					print_file(f'{indent1}}}', file = file)
				elif _mnem == 'esc':
					indent1 = indent1[:-1]
					print_file(f'{indent1}}}', file = file)

			if entry.kwds['mnem'] != 'ESC':
				print_file(f"{indent1}if(!execute)", file = file)
				print_file(f"{indent1}\treturn;", file = file)

			if modrm == 'mem':
				# Needed to fix IP-relative addresses
				print_file(f"{indent1}x86_calculate_operand_address(emu);", file = file)

				if not (entry.kwds['mnem'] in {'ADC', 'ADD', 'AND', 'BTC', 'BTR', 'BTS', 'DEC', 'INC', 'NEG', 'NOT', 'OR', 'SBB', 'SUB', 'XCHG', 'XOR'} and any('address' in opd for opd in opds) and modrm == 'mem') \
			and not (entry.kwds['mnem'] == 'MOV' and ('Cy' in entry.kwds['opds'] or 'Dy' in entry.kwds['opds'])):
					if mode != '87':
						print_file(f"{indent1}NO_LOCK();", file = file)
					# TODO
					#else:
					#	print_file(f"{indent1}if(sync)", file = file)
					#	print_file(f"{indent1}\tNO_LOCK();", file = file)
			else:
				print_file(f"{indent1}if(execute)", file = file)
				print_file(f"{indent1}\tNO_LOCK();", file = file)

			opds1 = opds.copy()
			mnem1 = entry.kwds['mnem']
			if mnem1[-1] in {'b', 'z', 'v'}:
				mnem1 = mnem1[:-1]
			if mnem1.startswith('J') and mnem1 not in INSTRUCTIONS:
				mnem1 = 'Jcc'
			if mnem1.startswith('CMOV') and mnem1 not in INSTRUCTIONS:
				mnem1 = 'CMOVcc'
			if mnem1.startswith('SET') and mnem1 not in INSTRUCTIONS:
				mnem1 = 'SETcc'
			if mnem1.startswith('LOOP') and mnem1 not in INSTRUCTIONS:
				mnem1 = 'LOOPcc'
			if mnem1.startswith('FCMOV') and mnem1 not in INSTRUCTIONS:
				mnem1 = 'FCMOVcc'
			if mnem1 in {'LDS', 'LES', 'LFS', 'LGS', 'LSS'}:
				# convert LxS a, b to MOV xS, a, b (NEC syntax)
				opds1.insert(0, OPERAND_CODE[mnem1[1:]])
				mnem1 = 'MOV'
			if mnem1 in INSTRUCTIONS:
				for parts, (line, fn, code) in INSTRUCTIONS[mnem1]:
					if parts != []:
						condition = False
						for part in parts:
							clause = True
							for morcel in part.split(' '):
								# TODO: need to add cpu and opcode (for 486A)
								if morcel.startswith('op='):
									if size[0] != morcel[3]:
										clause = False
										break
								elif morcel.startswith('mod='):
									if modrm is None or modrm[0] != morcel[4]:
										clause = False
										break
								elif morcel.startswith('cnt='):
									if len(opds1) != int(morcel[4:]):
										clause = False
										break
								elif morcel.startswith('op0='):
									if entry.kwds['opds'][0].lower() != morcel[4:]:
										clause = False
										break
								elif morcel.startswith('op1='):
									if entry.kwds['opds'][1].lower() != morcel[4:]:
										clause = False
										break
								else:
									print_file("// PARTS:", parts, file = file)
									clause = False
									break

							if clause:
								condition = True
								break

						if not condition:
							#print_file("// condition failed", size, parts, file = file)
							continue

					params = set(iter_params(code))
					kwds = {}
					size0 = size
					if mnem1 in {'LAR', 'LSL'}:
						size0 = max_long(size0) # TODO: make this explicit
					if size0 != 'void':
						kwds['opsize'] = size0[0]
					if mnem1.endswith('cc'):
						kwds['cond'] = entry.kwds['mnem'][len(mnem1) - 2:]

					if '$A' in params:
						assert '$S' not in params
						print_file(f"{indent1}switch(prs->address_size)", file = file)
						print_file(f"{indent1}{{", file = file)
						for adsize in ['word', 'long', 'quad']:
							kwds['adsize'] = adsize[0]
							print_file(f"{indent1}case SIZE_{get_bits(adsize)}BIT:", file = file)
							print_file(f"{indent1}\t{{", file = file)
							print_file(gen_code(line, fn, code, *opds1, indent = indent1 + '\t\t', **kwds), file = file)
							print_file(f"{indent1}\t}}", file = file)
							print_file(f"{indent1}\tbreak;", file = file)
						print_file(f"{indent1}default:", file = file)
						print_file(f"{indent1}\tassert(false);", file = file)
						print_file(f"{indent1}}}", file = file)
					elif '$S' in params:
						# TODO: $A as well as $S
						print_file(f"{indent1}switch(x86_get_stack_size(emu))", file = file)
						print_file(f"{indent1}{{", file = file)
						for stksize in ['word', 'long', 'quad']:
							kwds['stksize'] = stksize[0]
							print_file(f"{indent1}case SIZE_{get_bits(stksize)}BIT:", file = file)
							print_file(f"{indent1}\t{{", file = file)
							print_file(gen_code(line, fn, code, *opds1, indent = indent1 + '\t\t', **kwds), file = file)
							print_file(f"{indent1}\t}}", file = file)
							print_file(f"{indent1}\tbreak;", file = file)
						print_file(f"{indent1}default:", file = file)
						print_file(f"{indent1}\tassert(false);", file = file)
						print_file(f"{indent1}}}", file = file)
					else:
						print_file(f"{indent1}{{", file = file)

						if mode == '87' and not x87_async:
							print_file(f"{indent1}\tif(emu->x87.fpu_type == X87_FPU_INTEGRATED)", file = file)
							print_file(f"{indent1}\t\t_x87_int();", file = file)
							print_file(f"{indent1}\t_x87_busy();", file = file)
						if mode == '87' and (x87_async or mnem not in {'fldcw', 'fldenv', 'frstor', 'frstpm', 'fsetpm'}):
							print_file(f"{indent1}\tx87_store_exception_pointers(emu);", file = file)

						print_file(gen_code(line, fn, code, *opds1, indent = indent1 + '\t', **kwds), file = file)
						print_file(f"{indent1}}}", file = file)

					break # found pattern, no need to continue searching
				else:
					print_file("// MISSING CONDITIONS:", entry.kwds['mnem'], file = file)
			else:
				print_file("// MISSING INSTRUCTION:", entry.kwds['mnem'], file = file)
				MISSING.add(entry.kwds['mnem'])

			if cnt > 0:
				print_file(indent1[:-1] + '}', file = file)

		return True

	assert False

def print_single_implementation_entry(path, indent, actual_range, entry, discriminator, index, file, mode, uses_modrm = None, modrm = None, feature = None, only_mode = None):
	if entry is None:
		if mode == '32':
			print_file(f"{indent}UNDEFINED();", file = file)
		elif mode == '87':
			print_file(f"{indent}X87_UNDEFINED();", file = file)
		elif mode == '8':
			print_file(f"{indent}/* UNDEFINED(); */ // TODO", file = file)
		return True
	elif type(entry) is Table:
		return print_switch(mode, path + Path((index, discriminator),), indent, actual_range, file, discriminator = entry.kwds['discriminator'], uses_modrm = uses_modrm, modrm = modrm, feature = feature, only_mode = only_mode)
	else:
		return print_instruction(path, indent, actual_range, entry, discriminator, index, file, mode, modrm)

def get_implementation_ranges(mode, path, discriminator, index, actual_range, modrm = None):
	path1 = path + Path((index, discriminator))
	if mode == '32': # also includes 64-bit mode
		entry = X86_TABLE.lookup(path1)
	elif mode == '87':
		entry = X87_TABLE.lookup(path1)
	elif mode == '8':
		entry = X80_TABLE.lookup(path1)

	# Collect implementations according to introduction point
	impl_list = ([entry.tab] if entry.tab is not None else []) + entry.ins

	for impl in impl_list:
		flags = impl.kwds.get('flags', ())
		if 'only32bit' in flags:
			assert 'only64bit' not in flags

	intro_set = set()
	mode_set = set()
	for impl in impl_list:
		for cpu in actual_range:
			if cpu in impl.kwds['isas']:
				intro_set.add(cpu)
				break
		else:
			print(impl, actual_range)
			assert False

	intro_list = []
	for cpu in actual_range:
		if cpu in intro_set:
			intro_list.append(cpu)

	# order features: negated feature clauses should have their features appearing before
	feature_list = []
	feature_after = {}
	for impl in impl_list:
		feature_presence = None
		feature_absence = set()
		for feature_spec in impl.kwds.get('features', ()):
			if type(feature_spec) is tuple:
				feature_absence.add(feature_spec[1])
			else:
				assert feature_presence is None
				feature_presence = feature_spec

		if feature_presence in feature_after:
			feature_after[feature_presence].update(feature_absence)
		else:
			feature_after[feature_presence] = feature_absence

		if feature_presence in feature_list:
			for control_feature in feature_after[feature_presence]:#.copy():
				if control_feature not in feature_list:
					continue
				ix1 = feature_list.index(feature_presence)
				ix2 = feature_list.index(control_feature)
				assert ix2 < ix1
		else:
			feature_list.append(feature_presence)
			for prior_feature in feature_list:
				if feature_presence in feature_after[prior_feature]:
					feature_list.remove(prior_feature)
					feature_list.append(prior_feature)

	intro_impl_map = {}
	intro32_impl_map = {}
	intro64_impl_map = {}
	for intro in intro_list:
		for impl in impl_list:
			if intro in impl.kwds['isas']:

				for feature_spec in impl.kwds['features']:
					if type(feature_spec) is not tuple:
						feature = feature_spec
						break
				else:
					feature = None

				flags = impl.kwds.get('flags', ())
				if 'only32bit' in flags:
					for cpu1 in actual_range:
						if cpu1 in impl.kwds['isas']:
							if (cpu1, feature) not in intro32_impl_map:
								intro32_impl_map[cpu1, feature] = impl
							break
				elif 'only64bit' in flags:
					for cpu1 in actual_range:
						if cpu1 in impl.kwds['isas']:
							if (cpu1, feature) not in intro64_impl_map:
								intro64_impl_map[cpu1, feature] = impl
							break
				else:
					for cpu1 in actual_range:
						if cpu1 in impl.kwds['isas']:
							if (cpu1, feature) not in intro_impl_map:
								intro_impl_map[cpu1, feature] = impl
							break

	# check that all implementations are included
	for impl in impl_list:
		if impl not in intro_impl_map.values() and impl not in intro32_impl_map.values() and impl not in intro64_impl_map.values():
			print("Instruction ignored", path1, impl)

	for intro in intro_list:
		for feature in feature_list:
			if (intro, feature) in intro_impl_map:
				mode = None
				entry = intro_impl_map[intro, feature]
			elif (intro, feature) in intro32_impl_map:
				mode = '32'
				entry = intro32_impl_map[intro, feature]
			elif (intro, feature) in intro64_impl_map:
				mode = '64'
				entry = intro64_impl_map[intro, feature]
			else:
				continue

			impl_range = []
			for cpu in actual_range:
				if cpu in entry.kwds['isas']:
					impl_range.append(cpu)

			yield mode, impl_range, entry

def make_condition(mode, full_range, impl_range, features, imode):
	if mode == '32':
		cpu_prefix = 'X86_CPU_'
		cpu_type = 'cpu_type'
	elif mode == '87':
		cpu_prefix = 'X87_FPU_'
		cpu_type = 'fpu_type'
	elif mode == '8':
		cpu_prefix = 'X80_CPU_'
		cpu_type = 'cpu_type'
	else:
		assert False

	intervals = []
	interval_started = False
	for isa in full_range:
		if isa in impl_range:
			if interval_started:
				intervals[-1][1] = isa
			else:
				intervals.append([isa, isa])
			interval_started = True
		else:
			interval_started = False

	conditions = []
	for start, end in intervals:
		if start == full_range[0] and end == full_range[-1]:
			continue
		elif start == end:
			cpu_name = ARCHITECTURES[start].get('name', start).upper()
			condition = f"prs->{cpu_type} == {cpu_prefix}{cpu_name}"
		elif start == full_range[0]:
			cpu_name = ARCHITECTURES[end].get('name', end).upper()
			condition = f"prs->{cpu_type} <= {cpu_prefix}{cpu_name}"
		elif end == full_range[-1]:
			cpu_name = ARCHITECTURES[start].get('name', start).upper()
			condition = f"{cpu_prefix}{cpu_name} <= prs->{cpu_type}"
		else:
			start_cpu_name = ARCHITECTURES[start].get('name', start).upper()
			end_cpu_name = ARCHITECTURES[end].get('name', end).upper()
			condition = f"{cpu_prefix}{start_cpu_name} <= prs->{cpu_type} && prs->{cpu_type} <= {cpu_prefix}{end_cpu_name}"
		conditions.append(condition)

	if len(conditions) == 0:
		clauses = []
	elif len(conditions) == 1:
		clauses = [conditions[0]]
	else:
		for i in range(len(conditions)):
			if '&&' in conditions[i]:
				conditions[i] = '(' + conditions[i] + ')'
		clauses = [' || '.join(conditions)]

	if features is not None or imode is not None:
		if len(clauses) > 0 and '||' in clauses[0]:
			clauses[0] = '(' + clauses[0] + ')'

	if features is not None:
		for feature1 in features if type(features) is set else [features]:
			if type(feature1) is tuple:
				feature = feature1[1]
				negated = True
			else:
				feature = feature1
				negated = False
			description = FEATURE_NAMES[feature]

			if ',' in description:
				# take the first one as condition
				description = description.split(',')[0].strip()

			if description.startswith('CPU:'):
				cpu, cpu_flag = description[4:].split('/')
				op = "!=" if negated else "=="
				clause = f"prs->cpu_traits.cpu_subtype {op} X86_CPU_{cpu}_{cpu_flag}"
			elif description.startswith('FPU:'):
				cpu, cpu_flag = description[4:].split('/')
				op = "!=" if negated else "=="
				clause = f"prs->fpu_subtype {op} X87_FPU_{cpu}_{cpu_flag}"
			elif description.startswith('CPUID.'):
				clause = None
				for mode in description.split('/'):
					options = []
					# needed for CX8/CX16, "CPUID.EAX=00000001:EDX.CX8/CPUID.EAX=00000001:ECX.CX16"
					for part in mode.split('|'):
						# needed for SYSCALL, "CPUID.EAX=80000001:EDX.SYSCALL_K6|CPUID.EAX=80000001:EDX.SYSCALL"
						assert part.startswith('CPUID.')
						setup, check = part[6:].split(':')
						eax = None
						ecx = None
						for portion in setup.split('.'):
							# needed for FSGSBASE, "CPUID.EAX=00000007.ECX=00000000:EBX.FSGSBASE"
							if portion.startswith('EAX='):
								assert eax is None
								eax = int(portion[4:], 16)
								if eax >= 0x80000000:
									eax = f"_ext{eax - 0x80000000}"
								else:
									eax = str(eax)
							elif portion.startswith('ECX='):
								assert ecx is None
								ecx = int(portion[4:], 16)
							else:
								print(setup)
								assert False
						assert eax is not None
						setup = f"cpuid{eax}" + (f"_{ecx}" if ecx is not None else "")
						register, flag = check.split('.')
						try:
							# if it can be parsed as a decimal number, it is a bit offset, the name should be used
							int(flag, 10)
							flag = feature
						except:
							pass

						assert register in {'EAX', 'ECX', 'EDX', 'EBX'}
						checks = []

						options.append(f"(prs->cpu_traits.{setup}.{register.lower()} & X86_{setup.upper()}_{register}_{flag}) != 0")

					if len(options) > 1:
						clause1 = '(' + ' || '.join(options) + ')'
						if negated:
							clause1 = '!' + clause1
					else:
						clause1 = options[0]
						if negated:
							clause1 = '!(' + clause1 + ')'

					if clause is None:
						clause = clause1
					else:
						clause = f'(prs->operation_size != SIZE_64BIT ? {clause} : {clause1})'
			elif description == 'emulated':
				op = "!=" if negated else "=="
				clause = f"prs->cpu_method {op} X80_CPUMETHOD_EMULATED"
			else:
				op = "!" if negated else ""
				clause = f"{op}prs->cpu_traits.{description}"
			clauses.append(clause)

	if imode == '32':
		clauses.append("prs->code_size != SIZE_64BIT")
	elif imode == '64':
		clauses.append("prs->code_size == SIZE_64BIT")

	return ' && '.join(clauses)

def ranges(dim, start, end):
	for i in range(start, end):
		if dim == 1:
			yield (i,)
		else:
			for t in ranges(dim - 1, start, end):
				yield t + (i,)

def print_implementations(mode, path, discriminator, index, indent, actual_range, file, modrm, feature, only_mode):
	if_keyword = 'if'

	# We will maintain a grid of all possible (cpu, imode, features) tuples and check if everything is covered
	# To achieve this, we take account of all the possible cpu values (actual_range) and imode values (depends on mode and only_mode)
	# However, the number of features is potentially unbounded, so we will dynamically enlarge the table each time a new feature appears

	all_pairs = {
		(cpu, imode): ()
			for imode in ([only_mode] if only_mode is not None else ['32', '64'] if mode == '32' else ['32'])
				for cpu in actual_range
	}

	for imode, impl_range, entry in get_implementation_ranges(mode, path, discriminator, index, actual_range, modrm):
		if only_mode is not None and only_mode != imode:
			assert False

		# check if we have a new feature
		features = entry.kwds.get('features')
		new_feature = feature
		for feature_spec in features:
			if type(feature_spec) is not tuple:
				#print(new_feature, feature_spec)
				assert new_feature is None or new_feature == feature_spec
				new_feature = feature_spec

		condition = make_condition(mode, actual_range, impl_range, features, imode)
		uses_modrm = entry.kwds.get('modrm', False)
		if condition == '':
			if if_keyword == 'if':
				return print_single_implementation_entry(path, indent, impl_range, entry, discriminator, index, file, mode, uses_modrm = uses_modrm, modrm = modrm, feature = new_feature, only_mode = imode)
			else:
				print_file(indent + f"else({condition})", file = file)
		else:
			print_file(indent + f"{if_keyword}({condition})", file = file)
		print_file(indent + "{", file = file)
		print_single_implementation_entry(path, indent + "\t", impl_range, entry, discriminator, index, file, mode, uses_modrm = uses_modrm, modrm = modrm, feature = new_feature, only_mode = imode)
		print_file(indent + "}", file = file)

		for cpu in impl_range:
			for imode1 in [imode] if imode is not None else ['32', '64'] if mode == '32' else ['32']:
				latest_feature = None
				for feature_spec in features:
					if type(feature_spec) is not tuple:
						latest_feature = feature_spec
						break

				if latest_feature is not None:
					all_pairs[cpu, imode1] = latest_feature
				elif all_pairs[cpu, imode1] != () or len(features) == 0:
					if (cpu, imode1) not in all_pairs:
						print("Error, overloaded implementation for " + str(path + Path((index, discriminator))) + " on " + cpu, file = sys.stderr)
					del all_pairs[cpu, imode1]
				# otherwise, we only had negative conditions yet, so do not remove it

		if_keyword = 'else if'

	if mode != '8' and len(all_pairs) > 0:
		if if_keyword == 'if':
			print_file(indent + "UNDEFINED();", file = file)
		else:
			print_file(indent + "else", file = file)
			print_file(indent + "{", file = file)
			print_file(indent + "\tUNDEFINED();", file = file)
			print_file(indent + "}", file = file)

	return True

def print_switch(mode, path, indent, actual_range = None, file = None, discriminator = 'subtable', uses_modrm = None, modrm = None, feature = None, only_mode = None):
	assert mode in {'8', '32', '87'}
	if actual_range is None:
		if mode == '32':
			actual_range = [isa for isa in X86_ARCHITECTURE_LIST if ARCHITECTURES[isa].get('generate', 'yes') != 'no']
		elif mode == '8':
			actual_range = [isa for isa in X80_ARCHITECTURE_LIST if ARCHITECTURES[isa].get('generate', 'yes') != 'no']
		elif mode == '87':
			actual_range = [isa for isa in X87_ARCHITECTURE_LIST if ARCHITECTURES[isa].get('generate', 'yes') != 'no']

	# TODO: bad lock should not trigger UNDEFINED() on early CPUs (is this not DONE?)
	# TODO: remove 32/64-bit support when not present on later CPUs (386/amd64)
	if discriminator == 'subtable':
		if mode == '32':
			if len(path) == 0:
				print_file(indent + f"switch((opcode = x86_translate_opcode(prs, x86_fetch8(prs, emu))))", file = file)
			else:
				print_file(indent + f"switch((opcode = x86_fetch8(prs, emu)))", file = file)
		elif mode == '8':
			print_file(indent + f"switch((opcode = x80_fetch8(prs, emu)))", file = file)
		elif mode == '87':
			print_file(indent + f"switch(fop >> 8)", file = file)
		print_file(indent + "{", file = file)
		for index in range(256):
			print_file(indent + f"case 0x{index:02X}:", file = file)
			if print_implementations(mode, path, 'subtable', index, indent + "\t", actual_range, file, modrm, feature = feature, only_mode = only_mode):
				print_file(indent + "\tbreak;", file = file)
		print_file(indent + "}", file = file)
		return True
	elif discriminator == 'modfield':
		if modrm is None:
			assert uses_modrm is not None

			if mode != '87':
				print_file(f"{indent}prs->modrm_byte = x86_fetch8(prs, emu);", file = file)
			else:
				print_file(f"{indent}prs->modrm_byte = fop & 0xFF;", file = file)

			if uses_modrm:
				print_file(f"{indent}if((prs->modrm_byte & 0xC0) == 0xC0)", file = file)
				print_file(f"{indent}{{", file = file)
				print_implementations(mode, path, 'modfield', 1, indent + "\t", actual_range, file, modrm = 'reg', feature = feature, only_mode = only_mode)
				print_file(f"{indent}}}", file = file)
				print_file(f"{indent}else", file = file)
				print_file(f"{indent}{{", file = file)
				if mode != '87':
					print_file(f"{indent}\tx86_parse_modrm(prs, emu, execute);", file = file)
				print_implementations(mode, path, 'modfield', 0, indent + "\t", actual_range, file, modrm = 'mem', feature = feature, only_mode = only_mode)
				print_file(f"{indent}}}", file = file)
				return True
			else:
				print_file(f"// ONLY CHECK REGISTER {path}", file = file)
				modrm = 'reg'

		return print_implementations(mode, path, 'modfield', ['mem', 'reg'].index(modrm), indent, actual_range, file, modrm, feature = feature, only_mode = only_mode)
	elif discriminator == 'regfield':
		print_file(f"{indent}switch(REGFLDVAL(prs))", file = file)
		print_file(f"{indent}{{", file = file)
		for index1 in range(8):
			print_file(f"{indent}case {index1}:", file = file)
			if print_implementations(mode, path, 'regfield', index1, indent + "\t", actual_range, file, modrm, feature = feature, only_mode = only_mode):
				print_file(indent + "\tbreak;", file = file)
		print_file(f"{indent}default:", file = file)
		print_file(f"{indent}\tassert(false);", file = file)
		print_file(f"{indent}}}", file = file)
		return True
	elif discriminator == 'memfield':
		print_file(f"{indent}switch(MEMFLDVAL(prs))", file = file)
		print_file(f"{indent}{{", file = file)
		for index1 in range(8):
			print_file(f"{indent}case {index1}:", file = file)
			if print_implementations(mode, path, 'memfield', index1, indent + "\t", actual_range, file, modrm, feature = feature, only_mode = only_mode):
				print_file(indent + "\tbreak;", file = file)
		print_file(f"{indent}default:", file = file)
		print_file(f"{indent}\tassert(false);", file = file)
		print_file(f"{indent}}}", file = file)
		return True
	elif discriminator == 'prefix':
		# TODO: rewrite this
		print_file(f"{indent}switch(prs->simd_prefix)", file = file)
		print_file(f"{indent}{{", file = file)
		for index1, prefix in enumerate(['NONE', '66', 'F3', 'F2']):
			if prefix == 'NONE':
				print_file(f"{indent}default:", file = file)
			else:
				#try:
				#	imode, impl_range, entry = next(get_implementation_ranges(mode, path, 'prefix', index1, actual_range, modrm))
				#	if impl_range == actual_range and entry is None:
				#		continue
				#except StopIteration:
				#	continue
				print_file(f"{indent}case X86_PREF_{prefix}:", file = file)
			if print_implementations(mode, path, 'prefix', index1, indent + "\t", actual_range, file, modrm, feature = feature, only_mode = only_mode):
				print_file(indent + "\tbreak;", file = file)
		print_file(f"{indent}}}", file = file)
		return True
	else:
		print(discriminator)
		assert False

outfile = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(sys.argv[1])[0] + '.gen.c'

with open(outfile, 'w') as fp:
	print_file("static inline void x86_parse(x86_parser_t * prs, x86_state_t * emu, bool disassemble, bool execute)", file = fp)
	print_file("{", file = fp)
	print_file("\tuint8_t opcode;", file = fp)
	print_file("\tuoff_t opcode_offset;", file = fp)
	print_file("restart:", file = fp)
	print_file("\topcode_offset = prs->current_position;", file = fp)
	print_switch('32', Path(), '\t', file = fp)
	print_file("}", file = fp)

	print_file("static inline x86_result_t x80_parse(x80_parser_t * prs, x80_state_t * emu, x86_state_t * emu86, bool disassemble, bool execute)", file = fp)
	print_file("{", file = fp)
	print_file("\tuint16_t old_pc = prs->current_position;", file = fp)
	print_file("\tuint8_t opcode;", file = fp)
	print_switch('8', Path(), '\t', file = fp)
	print_file("\treturn X86_RESULT(X86_RESULT_SUCCESS, 0);", file = fp)
	print_file("}", file = fp)

	print_file("static inline void x87_parse(x86_parser_t * prs, x86_state_t * emu, bool sync, uint16_t fop, uint16_t fcs, uaddr_t fip, uint16_t fds, uaddr_t fdp, x86_segnum_t segment_number, uoff_t segment_offset, bool disassemble, bool execute)", file = fp)
	print_file("{", file = fp)
	print_file("\t_seg = segment_number;", file = fp)
	print_file("\t_off = segment_offset;", file = fp)
	print_switch('87', Path(), '\t', discriminator = 'subtable', file = fp)
	print_file("}", file = fp)

if len(MISSING) > 0:
	print("Missing operations: " + ', '.join(sorted(MISSING)))

cpuidmap = {}
for feature, description in FEATURE_NAMES.items():
	for part in description.split(','):
		part = part.strip()
		if part.startswith('CPUID.') and '/' not in part and '|' not in part:
			setup, check = part[6:].split(':')
			eax = None
			ecx = None
			for portion in setup.split('.'):
				# needed for FSGSBASE, "CPUID.EAX=00000007.ECX=00000000:EBX.FSGSBASE"
				if portion.startswith('EAX='):
					assert eax is None
					eax = int(portion[4:], 16)
					if eax >= 0x80000000:
						eax = f"_ext{eax - 0x80000000}"
					else:
						eax = str(eax)
				elif portion.startswith('ECX='):
					assert ecx is None
					ecx = int(portion[4:], 16)
				else:
					print(setup)
					assert False
			assert eax is not None
			setup = f"cpuid{eax}" + (f"_{ecx}" if ecx is not None else "")

			if '"' in check:
				flag_name = check[check.find('"') + 1:]
				if '"' in flag_name:
					flag_name = flag_name[:flag_name.find('"')]
				flag_name = flag_name.strip()
				check = check[:check.find('"')].strip()
			else:
				flag_name = feature

			register, flag = check.split('.')
			try:
				# if it can be parsed as a decimal number, it is a bit offset, the name should be used
				bitnumber = int(flag, 10)
			except:
				continue

			assert register in {'EAX', 'ECX', 'EDX', 'EBX'}
			checks = []

			entry = (setup, register.lower(), flag_name.lower(), bitnumber)

			if feature.lower() not in cpuidmap:
				cpuidmap[feature.lower()] = []
			cpuidmap[feature.lower()].append(entry)
		elif not part.startswith('CPU:') and not part.startswith('FPU:') and not part == 'emulated':
			entry = part

			if feature.lower() not in cpuidmap:
				cpuidmap[feature.lower()] = []
			cpuidmap[feature.lower()].append(entry)

features = {}
for architecture in PROCESSORS:
	archid = architecture['id']
	featureset = set()
	if architecture.get('features', '') != '':
		for feature in architecture['features'].split(','):
			feature = feature.strip()
			if feature.startswith('$'):
				#print(feature)
				assert feature[1:] in features
				featureset.update(features[feature[1:]])
			else:
				featureset.add(feature)
		for feature in featureset.copy():
			if feature.startswith('-'):
				featureset.remove(feature)
				featureset.remove(feature[1:])
	features[archid] = featureset

_ALLFEATURES = set()

fputypes = {}
for architecture in PROCESSORS:
	if architecture.get('type', 'cpu') == 'fpu':
		for alias in architecture.get('aliases', '').split(','):
			alias = alias.strip()
			if alias == '':
				continue
			fputypes[alias] = architecture
		fputypes[architecture['id']] = architecture

outfile = os.path.splitext(sys.argv[1])[0] + '.list.c'

with open(outfile, 'w') as file:
	print("""typedef enum x86_cpu_version_t
{""", file = file)
	for architecture in PROCESSORS:
		if architecture.get('type', 'cpu') == 'cpu':
			print(f"\tX86_CPU_TYPE_{architecture['id'].upper()},", file = file)
	print("} x86_cpu_version_t;", file = file)

	print("x86_cpu_traits_t x86_cpu_traits[] =\n{", file = file)
	for architecture in PROCESSORS:
		if architecture.get('type', 'cpu') == 'cpu':
			featureset = features[architecture['id']]
			noncpuid_features = set()
			cpuid_values = {}
			for feature in featureset:
				if feature in cpuidmap:
					for entry in cpuidmap[feature]:
						if type(entry) is str:
							noncpuid_features.add(entry)
						else:
							leaf, reg, name, bit = entry
							name = f'X86_{leaf}_{reg}_{name}'.upper()
							key = (leaf, reg)
							if key in cpuid_values:
								cpuid_values[key].add((bit, name))
							else:
								cpuid_values[key] = {(bit, name)}
				else:
					_ALLFEATURES.add(feature)
			cpuid_feature_list = []
			current_leaf = None
			registers = {}
			for leaf, reg in sorted(cpuid_values.keys()):
				if current_leaf is None:
					current_leaf = leaf
					registers = {}
				elif current_leaf != leaf:
					cpuid_feature_list.append((current_leaf, registers))
					current_leaf = leaf
					registers = {}
				registers[reg] = cpuid_values[leaf, reg]
			if current_leaf is not None:
				cpuid_feature_list.append((current_leaf, registers))

			feature_sequence = ""
			if 'variant' in architecture:
				feature_sequence += f"""\n\t\t\t.cpu_subtype = X86_CPU_{architecture['class'].upper()}_{architecture['variant'].upper()},"""
			if 'fpu' in architecture:
				fpu_list = []
				for fpu in architecture['fpu'].split(','):
					fpu = fpu.strip()
					if fpu == '':
						continue
					fpu = fputypes.get(fpu, fpu) # get FPU structure from alias
					fpu = fpu['class'] # get FPU class
					fpu = ARCHITECTURES[fpu].get('name', fpu)
					if len(fpu_list) == 0:
						feature_sequence += f"""\n\t\t\t.default_fpu = X87_FPU_{fpu.upper()},"""
					fpu_list.append(f"(1 << X87_FPU_{fpu.upper()})")
				feature_sequence += f"""\n\t\t\t.supported_fpu_types = {' | '.join(fpu_list)},"""

			if 'cpuid' in featureset:
				if 'highest_function' in architecture:
					highest_function_number = int(architecture['highest_function'], 16)
					highest_function = f"\t\t\t\t.eax = 0x{highest_function_number:08X},\n"
				else:
					highest_function = "\t\t\t\t.eax = 0x00000001, // TODO\n"
				feature_sequence += f"""\n\t\t\t.cpuid0 =
			{{
{highest_function}				X86_CPUID_VENDOR_{architecture['vendor'].upper()},
			}},"""

			for leaf, registers in cpuid_feature_list:
				for reg in registers.keys():
					registers[reg] = ' | '.join([bit_name[1] for bit_name in sorted(registers[reg], key = lambda bit_name: bit_name[0])])

			if 'family' in architecture:
				family = int(architecture['family'])
				if family > 15:
					info = ((family - 15) << 20) | (15 << 8)
				else:
					info = family << 8
				if 'model' in architecture:
					model = int(architecture['model'])
					info |= (model & 0xF) << 4
					info |= (model & 0xF0) << (16 - 2)
				if 'stepping' in architecture:
					stepping = int(architecture['stepping'])
					info |= stepping & 0xF
				info_text = f'0x{info:08X}'
				for i in range(len(cpuid_feature_list)):
					if cpuid_feature_list[i][0] == 'cpuid1':
						cpuid_feature_list[i][1]['eax'] = info_text
						break
					elif cpuid_feature_list[i][0] > 'cpuid1':
						cpuid_feature_list.insert(i, ('cpuid1', {'eax': info_text}))
						break
				else:
					cpuid_feature_list.append(('cpuid1', {'eax': info_text}))

			if 'highest_extended_function' in architecture:
				highest_extended_function_number = 0x80000000 + int(architecture['highest_extended_function'], 16)
				highest_extended_function = f"\t\t\t\t.eax = 0x{highest_extended_function_number:08X},\n"
				cpuid_feature_list.append(('cpuid_ext0', {'eax': f'0x{highest_extended_function_number:08X}'}))

			for leaf, registers in cpuid_feature_list:
				feature_sequence += f"""\n\t\t\t.{leaf} =
			{{""" + ''.join(f'\n\t\t\t\t.{reg} = ' + value + ',' for reg, value in sorted(registers.items())) + """
			},"""

			feature_sequence += ''.join(f'\n\t\t\t.{feature} = true,' for feature in sorted(noncpuid_features))

			description = architecture['description'].replace('\"', '\\\"')
			arch_class = architecture['class']
			arch_class = ARCHITECTURES[arch_class].get('name', arch_class)
			print(f"""	[X86_CPU_TYPE_{architecture['id'].upper()}] =
		{{
			.description = \"{description}\",
			.cpu_type = X86_CPU_{arch_class.upper()},{feature_sequence}
		}},
""", file = file)
	print("};", file = file)

	print("""\tstatic const struct
{
	const char * name;
	x86_cpu_version_t type;
} supported_cpu_table[] =
{""", file = file)

	for architecture in PROCESSORS:
		if architecture.get('type', 'cpu') == 'cpu':
			aliases = set()
			for alias in architecture.get('aliases', '').split(','):
				alias = alias.strip()
				if alias == '':
					continue
				print(f'\t{{ "{alias}", X86_CPU_TYPE_{architecture["id"].upper()} }},', file = file)
				aliases.add(alias)
			if architecture["id"].lower() not in aliases:
				print(f'\t{{ "{architecture["id"].lower()}", X86_CPU_TYPE_{architecture["id"].upper()} }},', file = file)
	print("};", file = file)

	print("""\tstatic const struct
{
	const char * name;
	const char * description;
	x87_fpu_type_t type;
	x87_fpu_subtype_t subtype;
} supported_fpu_table[] =
{""", file = file)

	for architecture in PROCESSORS:
		if architecture.get('type', 'cpu') == 'fpu':
			if 'variant' in architecture:
				variant = f', X87_FPU_{architecture["class"].upper()}_{architecture["variant"].upper()}'
			else:
				variant = ', 0'
			description = architecture['description'].replace('\"', '\\\"')
			aliases = set()
			arch_class = architecture['class']
			arch_class = ARCHITECTURES[arch_class].get('name', arch_class)
			for alias in architecture.get('aliases', '').split(','):
				alias = alias.strip()
				if alias == '':
					continue
				print(f'\t{{ "{alias}", "{description}", X87_FPU_{arch_class.upper()}{variant} }},', file = file)
				aliases.add(alias)
			if architecture["id"].lower() not in aliases:
				print(f'\t{{ "{architecture["id"].lower()}", "{description}", X87_FPU_{arch_class.upper()}{variant} }},', file = file)
	print("};", file = file)

print("Missing features:", sorted(_ALLFEATURES))



//// Accessing registers

static inline void x86_set_xip(x86_state_t * emu, uoff_t value)
{
	emu->restarted_instruction.opcode = 0;
	emu->xip = value;
	x86_prefetch_queue_flush(emu);
}

// General purpose registers

static inline uint8_t x86_register_get8_low(x86_state_t * emu, int number)
{
	return emu->gpr[number] & 0xFF;
}

static inline uint8_t x86_register_get8_high(x86_state_t * emu, int number)
{
	return (emu->gpr[number] >> 8) & 0xFF;
}

// This method is useful for ModRM or opcode+REG types of access, otherwise x86_register_get8_high can be used
static inline uint8_t x86_register_get8(x86_state_t * emu, int number)
{
	if(emu->parser->rex_prefix || (number & 4) == 0)
		return x86_register_get8_low(emu, number);
	else
		return x86_register_get8_high(emu, number & 3);
}

static inline void x86_register_set8_low(x86_state_t * emu, int number, uint8_t value)
{
	emu->gpr[number] = (emu->gpr[number] & ~0xFF) | value;
}

static inline void x86_register_set8_high(x86_state_t * emu, int number, uint8_t value)
{
	emu->gpr[number] = (emu->gpr[number] & ~0xFF00) | (value << 8);
}

// This method is useful for ModRM or opcode+REG types of access, otherwise x86_register_set8_high can be used
static inline void x86_register_set8(x86_state_t * emu, int number, uint8_t value)
{
	if(emu->parser->rex_prefix || (number & 4) == 0)
		x86_register_set8_low(emu, number, value);
	else
		x86_register_set8_high(emu, number & 3, value);
}

static inline uint16_t x86_register_get16(x86_state_t * emu, int number)
{
	return emu->gpr[number] & 0xFFFF;
}

static inline void x86_register_set16(x86_state_t * emu, int number, uint16_t value)
{
	emu->gpr[number] = (emu->gpr[number] & ~0xFFFF) | value;
}

static inline uint32_t x86_register_get32(x86_state_t * emu, int number)
{
	return emu->gpr[number] & 0xFFFFFFFF;
}

// 32-bit register writes clear the entire register
static inline void x86_register_set32(x86_state_t * emu, int number, uint32_t value)
{
	// 32-bit register assignments clear the high bits
	emu->gpr[number] = value;
}

static inline uint64_t x86_register_get64(x86_state_t * emu, int number)
{
	return emu->gpr[number];
}

static inline void x86_register_set64(x86_state_t * emu, int number, uint64_t value)
{
	emu->gpr[number] = value;
}

// Segment registers

// Updates the selector and base fields, according to real mode segmentation rules
// Note that in protected/long mode, this requires access to other resources, those functions are included in protection.c
static inline void x86_segment_load_real_mode(x86_state_t * emu, x86_segnum_t segment, uint16_t value)
{
	emu->sr[segment].selector = value;
	if((emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED) && (segment == X86_R_DS3 || segment == X86_R_DS2))
	{
		// V55 extended registers provide access to a 24-bit address space
		emu->sr[segment].base = (uint32_t)value << 8;
	}
	else
	{
		emu->sr[segment].base = (uint32_t)value << 4;
	}

	if(segment == X86_R_CS)
	{
		// according to Robert Collins
		emu->sr[segment].limit = 0xFFFF;
		emu->sr[segment].access &= ~0x00400000; /* preserve default width size */
		emu->sr[segment].access |= 0x00009300; /* read/write cpl=0 data */
	}
}

// Forces the segment register to real mode access, useful for virtual 8086 mode
static inline void x86_segment_load_real_mode_full(x86_state_t * emu, x86_segnum_t segment, uint16_t value)
{
	x86_segment_load_real_mode(emu, segment, value);
	emu->sr[segment].limit = 0xFFFF;
	emu->sr[segment].access = 0x0000F300; /* read/write cpl=3 data */
}

static inline x86_segnum_t x86_segment_get_number(x86_state_t * emu, x86_segnum_t segment_number)
{
	if(emu->cpu_type == X86_CPU_8086
		|| (X86_CPU_V60 <= emu->cpu_type && emu->cpu_type <= X86_CPU_V25)) // this is a guess
	{
		segment_number &= 3;
	}
	else
	{
		segment_number &= 7;
		switch(segment_number)
		{
		case X86_R_FS:
		case X86_R_GS:
			if(emu->cpu_type == X86_CPU_V55)
				segment_number |= 4; // this is a guess
			else if(emu->cpu_type < X86_CPU_386)
				x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
			break;
		case X86_R_DS2:
		case X86_R_DS3:
			if(emu->cpu_type != X86_CPU_V55 && emu->cpu_type != X86_CPU_EXTENDED)
				x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
			break;
		default:
			break;
		}
	}
	return segment_number;
}

static inline uint16_t x86_segment_get(x86_state_t * emu, x86_segnum_t segment_number)
{
	return emu->sr[x86_segment_get_number(emu, segment_number)].selector;
}

// Note: x86_segment_set relies on memory protection, so it is moved there

// V25/V55 register banks

static inline int x86_register_bank_number(x86_state_t * emu, int value)
{
	if(emu->cpu_type == X86_CPU_V25)
		return value & 7;
	else
		return value & 0xF;
}

static inline int x86_retrieve_register_bank_number(x86_state_t * emu, uint16_t flags)
{
	return x86_register_bank_number(emu, flags >> 4);
}

// Synchronizes V25/V55 banks to register values
static inline void x86_store_register_bank(x86_state_t * emu)
{
	int word_index;
	if(emu->cpu_type == X86_CPU_V55)
	{
		for(word_index = 0; word_index < 2; word_index++)
		{
			emu->bank[emu->rb].w[word_index] = htole16(emu->sr[7 - word_index].selector);
		}
	}
	for(word_index = 4; word_index < 8; word_index++)
	{
		emu->bank[emu->rb].w[word_index] = htole16(emu->sr[7 - word_index].selector);
	}
	for(word_index = 8; word_index < 16; word_index++)
	{
		emu->bank[emu->rb].w[word_index] = htole16(x86_register_get16(emu, 15 - word_index));
	}
}

// Synchronizes register values to V25/V55 banks
static inline void x86_load_register_bank(x86_state_t * emu)
{
	int word_index;
	if(emu->cpu_type == X86_CPU_V55)
	{
		for(word_index = 0; word_index < 2; word_index++)
		{
			x86_segment_load_real_mode(emu, 7 - word_index, le16toh(emu->bank[emu->rb].w[word_index]));
		}
	}
	for(word_index = 4; word_index < 8; word_index++)
	{
		x86_segment_load_real_mode(emu, 7 - word_index, le16toh(emu->bank[emu->rb].w[word_index]));
	}
	for(word_index = 8; word_index < 16; word_index++)
	{
		x86_register_set16(emu, 15 - word_index, le16toh(emu->bank[emu->rb].w[word_index]));
	}
}

static inline void x86_set_register_bank_number(x86_state_t * emu, int number)
{
	x86_store_register_bank(emu);
	emu->rb = x86_register_bank_number(emu, number);
	x86_load_register_bank(emu);
}

// FLAGS register

/*
	The following flags are defined (using Intel mnemonics where they exist)

	        21  20  19  18  17  16  15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
	8086    -   -   -   -   -   -   1   1   1   1   OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
	V20     -   -   -   -   -   -   MD  1   1   1   OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
	µPD9002 -   -   -   -   -   -   MD  1   1   1   OF  DF  IF  TF  SF  ZF  X5  AF  X3  PF  NF  CF
	V25     -   -   -   -   -   -   MD* [RB       ] OF  DF  IF  TF  SF  ZF  F1  AF  F0  PF  ^IBRK CF	*different interpretation (TODO: unimplemented)
	V55     -   -   -   -   -   -   [RB           ] OF  DF  IF  TF  SF  ZF  0   AF  0   PF  ^IBRK CF
	286     -   -   -   -   -   -   0   NT  [IOPL ] OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
	386     0   0   0   0   VM  RF  0   NT  [IOPL ] OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
	486     0   0   0   AC  VM  RF  0   NT  [IOPL ] OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
	586     ID  VIP VIF AC  VM  RF  0   NT  [IOPL ] OF  DF  IF  TF  SF  ZF  0   AF  0   PF  1   CF
*/

// TODO: on the µPD9002, it is not clear if the Z80 specific NF, X3, X5 flags are accessible as part of the FLAGS register

// These functions get/set the full FLAGS register, which is not the usual behavior for user level instructions such as PUSHF/POPF, but required for interrupt calls and returns
static inline uint8_t x86_flags_get8(x86_state_t * emu)
{
	uint8_t flags = emu->cf | emu->pf | emu->af | emu->zf | emu->sf;
	if(emu->cpu_type == X86_CPU_UPD9002)
		return flags | emu->z80_flags; // I'm guessing µPD9002 stores the Z80 F register in FLAGS, for example on the stack image during interrupts
	else if(emu->cpu_type == X86_CPU_V25 || emu->cpu_type == X86_CPU_EXTENDED)
		return flags | emu->ibrk_ | (emu->iram[X86_SFR_FLAG] & X86_FLAG_MASK); // The F0, F1 flags are stored in the SFR called FLAG
	else if(emu->cpu_type == X86_CPU_V55)
		return flags | emu->ibrk_;
	else
		return flags | 2; // On all other CPUs, bit 2 is always set
}

static inline void x86_flags_set8(x86_state_t * emu, uint8_t value)
{
	emu->cf = value & X86_FL_CF;
	if(emu->cpu_type == X86_CPU_V25 || emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED)
	{
		emu->ibrk_ = value & X86_FL_IBRK_;
	}
	emu->pf = value & X86_FL_PF;
	if(emu->cpu_type == X86_CPU_V25)
	{
		emu->iram[X86_SFR_FLAG] = value & X86_FLAG_MASK;
	}
	emu->af = value & X86_FL_AF;
	emu->zf = value & X86_FL_ZF;
	emu->sf = value & X86_FL_SF;

	if(emu->cpu_type == X86_CPU_UPD9002)
		emu->z80_flags = value & 0x2A;
}

static inline uint16_t x86_flags_get16(x86_state_t * emu)
{
	uint16_t flags = x86_flags_get8(emu) | emu->tf | emu->_if | emu->df | emu->of;
	if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_186 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_V60)
	{
		return flags | 0xF000;
	}
	else if(emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002)
	{
		return flags | emu->md | 0x7000;
	}
	else if(emu->cpu_type == X86_CPU_V25)
	{
		return flags | (emu->rb << X86_FL_RB_SHIFT) | emu->md;
	}
	else if(emu->cpu_type == X86_CPU_V55)
	{
		return flags | (emu->rb << X86_FL_RB_SHIFT);
	}
	else if(emu->cpu_type == X86_CPU_EXTENDED)
	{
		return flags | (emu->iopl << X86_FL_IOPL_SHIFT) | emu->nt | emu->md; // extension, supports 286 flag and the negation of the mode flag
	}
	else
	{
		return flags | (emu->iopl << X86_FL_IOPL_SHIFT) | emu->nt;
	}
}

static inline void x86_flags_set16(x86_state_t * emu, uint16_t value)
{
	// If the RB bits change, a bank switch occurs, so we need to synchronize the registers with the banks
	bool switch_banks = false;
	switch(emu->cpu_type)
	{
	case X86_CPU_V25:
		switch_banks = emu->rb != ((value & X86_FL_V25_RB_MASK) >> X86_FL_RB_SHIFT);
		break;
	case X86_CPU_V55:
		switch_banks = emu->rb != ((value & X86_FL_V55_RB_MASK) >> X86_FL_RB_SHIFT);
		break;
	default:
		break;
	}

	if(switch_banks)
	{
		x86_store_register_bank(emu);
	}

	x86_flags_set8(emu, value);
	emu->tf = value & X86_FL_TF;
	emu->_if = value & X86_FL_IF;
	emu->df = value & X86_FL_DF;
	emu->of = value & X86_FL_OF;
	if((emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002 || emu->cpu_type == X86_CPU_EXTENDED) && emu->md_enabled)
	{
		emu->md = value & X86_FL_MD; // The mode flag can only be altered if it is write enabled
	}
	if(emu->cpu_type == X86_CPU_V25)
	{
		emu->rb = (value & X86_FL_V25_RB_MASK) >> X86_FL_RB_SHIFT;
		if(emu->cpu_traits.cpu_subtype == X86_CPU_V25_V25S)
			emu->md = value & X86_FL_MD;
	}
	else if(emu->cpu_type == X86_CPU_V55)
	{
		emu->rb = (value & X86_FL_V55_RB_MASK) >> X86_FL_RB_SHIFT;
	}
	else if(emu->cpu_type >= X86_CPU_286)
	{
		emu->iopl = (value >> X86_FL_IOPL_SHIFT) & 3;
		emu->nt = value & X86_FL_NT;
	}

	if(switch_banks)
	{
		x86_load_register_bank(emu);
	}
}

static inline uint32_t x86_flags_get32(x86_state_t * emu)
{
	uint32_t eflags = (uint32_t)x86_flags_get16(emu) | emu->rf;
	if(!(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376))
	{
		eflags |= emu->vm;
	}
	if(emu->cpu_type >= X86_CPU_486)
	{
		eflags |= emu->ac;
	}
	if(emu->cpu_type >= X86_CPU_586)
	{
		eflags |= emu->vif | emu->vip | emu->id;
	}
	return eflags;
}

static inline void x86_flags_set32(x86_state_t * emu, uint32_t value)
{
	x86_flags_set16(emu, (uint16_t)value);
	emu->rf = value & X86_FL_RF;
	if(!(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376))
	{
		emu->vm = value & X86_FL_VM;
	}
	if(emu->cpu_type >= X86_CPU_486)
	{
		emu->ac = value & X86_FL_AC;
	}
	if(emu->cpu_type >= X86_CPU_586)
	{
		emu->vif = value & X86_FL_VIF;
		emu->vip = value & X86_FL_VIP;
		emu->id = value & X86_FL_ID;
	}
}

inline uint64_t x86_flags_get64(x86_state_t * emu)
{
	return x86_flags_get32(emu);
}

inline void x86_flags_set64(x86_state_t * emu, uint64_t value)
{
	x86_flags_set32(emu, value);
}

// These functions get/set the FLAGS register as accessed by user mode instructions, such as PUSHF/POPF

static inline uint8_t x86_flags_update_image8(x86_state_t * emu, uint8_t flags)
{
	if(emu->cpu_type == X86_CPU_UPD9002)
		flags = (flags & ~0x28) | 2; // I'm guessing that in µPD9002, normally you cannot access the NF, X3, X5 flags to stay compatible with the V20

	return flags;
}

static inline uint8_t x86_flags_get_image8(x86_state_t * emu)
{
	return x86_flags_update_image8(emu, x86_flags_get8(emu));
}

static inline uint8_t x86_flags_fix_image8(x86_state_t * emu, uint8_t flags)
{
	if(emu->cpu_type == X86_CPU_UPD9002)
		flags = (flags & ~0x2A) | emu->z80_flags;

	return flags;
}

static inline void x86_flags_set_image8(x86_state_t * emu, uint8_t value)
{
	x86_flags_set8(emu, x86_flags_fix_image8(emu, value));
}

static inline uint16_t x86_flags_update_image16(x86_state_t * emu, uint16_t flags)
{
	flags = (flags & ~0xFF) | x86_flags_update_image8(emu, flags);

	if(x86_is_virtual_8086_mode(emu) && emu->iopl < 3)
	{
		if(emu->vif)
			flags |= X86_FL_IF;
		else
			flags &= ~X86_FL_IF;
		flags |= X86_FL_IOPL_MASK;
		/* note: only the low 16 bits are available */
	}

	return flags;
}

static inline uint16_t x86_flags_get_image16(x86_state_t * emu)
{
	return x86_flags_update_image16(emu, x86_flags_get16(emu));
}

static inline uint16_t x86_flags_fix_image16(x86_state_t * emu, uint16_t flags)
{
	if(x86_is_ia64(emu) && (flags & (X86_FL_IF | X86_FL_TF)) != (emu->_if | emu->tf))
		x86_ia64_intercept(emu, 0); // TODO

	flags = (flags & ~0xFF) | x86_flags_fix_image8(emu, flags);

	if(emu->cpu_type == X86_CPU_V60)
	{
		if((flags & X86_FL_IF) != emu->_if)
		{
			x86_set_xip(emu, emu->old_xip + 1);
			x86_v60_exception(emu, V60_EXC_PI);
		}
		if((flags & X86_FL_TF) != emu->tf)
		{
			x86_v60_exception(emu, V60_EXC_SST);
		}
	}

	if(((x86_is_virtual_8086_mode(emu) && (emu->cr[4] & X86_CR4_VME) != 0) || x86_is_protected_mode(emu)) && emu->iopl < x86_get_cpl(emu))
	{
		flags = (flags & ~X86_FL_IF) | emu->_if;
	}

	// TODO: where is the 286 behavior documented?
	if(x86_get_cpl(emu) != 0 || (emu->cpu_type == X86_CPU_286 && x86_is_real_mode(emu)))
	{
		flags = (flags & ~X86_FL_IOPL_MASK) | ((emu->iopl) << X86_FL_IOPL_SHIFT);
	}

	// TODO: where is the 286 behavior documented?
	if(emu->cpu_type == X86_CPU_286 && x86_is_real_mode(emu))
	{
		flags = (flags & ~X86_FL_NT) | emu->nt;
	}

	if(emu->cpu_type == X86_CPU_V25)
	{
		flags = (flags & ~X86_FL_V25_RB_MASK) | ((emu->rb << X86_FL_RB_SHIFT) & X86_FL_V25_RB_MASK);
	}
	else if(emu->cpu_type == X86_CPU_V55)
	{
		flags = (flags & ~X86_FL_V55_RB_MASK) | ((emu->rb << X86_FL_RB_SHIFT) & X86_FL_V55_RB_MASK);
	}

	if((emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002 || emu->cpu_type == X86_CPU_EXTENDED) && !emu->md_enabled)
	{
		flags = (flags & ~X86_FL_MD) | emu->md;
	}

	flags &= ~X86_FL_RF;

	return flags;
}

static inline void x86_flags_set_image16(x86_state_t * emu, uint16_t value)
{
	x86_flags_set16(emu, x86_flags_fix_image16(emu, value));
}

static inline uint32_t x86_flags_update_image32(x86_state_t * emu, uint32_t flags)
{
	flags = (flags & ~0xFFFF) | x86_flags_update_image16(emu, flags);
	flags &= ~X86_FL_VM;
	flags &= ~X86_FL_RF;

	return flags;
}

static inline uint32_t x86_flags_get_image32(x86_state_t * emu)
{
	return x86_flags_update_image32(emu, x86_flags_get32(emu));
}

static inline uint32_t x86_flags_fix_image32(x86_state_t * emu, uint32_t flags)
{
	flags = (flags & ~0xFFFF) | x86_flags_fix_image16(emu, flags);

	if(x86_is_ia64(emu) && (flags & X86_FL_AC) != emu->ac)
		x86_ia64_intercept(emu, 0); // TODO

	flags = (flags & ~X86_FL_VM) | emu->vm;
	if(x86_is_virtual_8086_mode(emu) && (emu->cr[4] & X86_CR4_VME) != 0 && emu->iopl < 3)
	{
		flags = (flags & ~X86_FL_VIF) | (emu->_if ? X86_FL_VIF : 0);
	}
	flags = (flags & ~X86_FL_VIP) | emu->vip;

	return flags;
}

static inline void x86_flags_set_image32(x86_state_t * emu, uint32_t value)
{
	x86_flags_set32(emu, x86_flags_fix_image32(emu, value));
}

static inline uint64_t x86_flags_get_image64(x86_state_t * emu)
{
	return x86_flags_get_image32(emu);
}

static inline void x86_flags_set_image64(x86_state_t * emu, uint64_t value)
{
	x86_flags_set_image32(emu, value);
}

// Control registers

static inline uint32_t x86_control_register_get32(x86_state_t * emu, int number)
{
	// TODO: which flags are allowed, which CPU introduced it
	switch(number)
	{
	case 0:
	case 2:
	case 3:
		if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		// TODO: other CPUs
		break;
	case 4:
		if(emu->cpu_type <= X86_CPU_486)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		// TODO: other CPUs
		break;
	case 8:
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	return emu->cr[number];
}

static inline uint64_t x86_control_register_get64(x86_state_t * emu, int number)
{
	// TODO: which flags are allowed, which CPU introduced it
	switch(number)
	{
	case 0:
	case 2:
	case 3:
	case 4:
	case 8:
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	return emu->cr[number];
}

static inline void x86_control_register_set32(x86_state_t * emu, int number, uint32_t value)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	switch(number)
	{
	case 0:
		if(emu->cpu_type == X86_CPU_386)
		{
			if(emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
				value = (value & 0x0000001F) | 0x00000011;
			else
				value &= 0x8000001F;
		}
		else
		{
			// TODO: from Pentium, assignment to these bits should trigger an interrupt
			value &= 0xE005003F;
		}

		if((emu->cr[4] & X86_CR4_PAE) == 0 && (emu->efer & X86_EFER_LME) != 0
		&& (value & X86_CR0_PG) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

		if((x86_is_64bit_mode(emu) || (emu->cr[4] & X86_CR4_PCIDE) != 0)
		&& (value & X86_CR0_PG) == 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	case 2:
		if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		// TODO: other CPUs
		break;
	case 3:
		if(emu->cpu_type == X86_CPU_386)
		{
			if(emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
				x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
			else
				value &= 0xFFFFF000;
		}
		else if(emu->cpu_type == X86_CPU_486)
		{
			value &= 0xFFFFF018;
		}
		// TODO: other CPUs
		break;
	case 4:
		if(emu->cpu_type <= X86_CPU_486)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

		if((emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_PAE) == 0)
			value &= ~X86_CR4_PAE;

		// Pentium: value &= 0x0000005F
		// Pro: PAE, PGE, PCE
		// TODO: other CPUs

		if((emu->cr[0] & X86_CR0_PG) != 0 && (emu->efer & X86_EFER_LME) != 0
		&& (value & (X86_CR4_PAE | X86_CR4_VA57)) != (emu->cr[4] & (X86_CR4_PAE | X86_CR4_VA57)))
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	case 8:
		if(emu->cpu_type == X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		// TODO: other CPUs
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}

	emu->cr[number] = value;

	if(number == 0 || number == 4)
	{
		// TODO: is this the correct behavior?
		if((emu->cr[0] & X86_CR0_PG) != 0 && (emu->cr[4] & X86_CR4_PAE) == 0)
			value &= ~X86_EFER_LME;
	}
	if(number == 0)
	{
		// TODO: is this the correct behavior?
		if((emu->efer & X86_EFER_LME) != 0 && (emu->cr[0] & X86_CR0_PG) != 0)
			emu->efer |= X86_EFER_LMA;
		else
			emu->efer &= ~X86_EFER_LMA;
	}
}

static inline void x86_control_register_set64(x86_state_t * emu, int number, uint64_t value)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	switch(number)
	{
	case 0:
		if((emu->cr[4] & X86_CR4_PAE) == 0 && (emu->efer & X86_EFER_LME) != 0
		&& (value & X86_CR0_PG) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

		if((x86_is_64bit_mode(emu) || (emu->cr[4] & X86_CR4_PCIDE) != 0)
		&& (value & X86_CR0_PG) == 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		if((emu->cr[0] & X86_CR0_PG) != 0 && (emu->efer & X86_EFER_LME) != 0
		&& (emu->cr[4] & (X86_CR4_PAE | X86_CR4_VA57)) != (value & (X86_CR4_PAE | X86_CR4_VA57)))
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	case 8:
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}

	emu->cr[number] = value;

	if(number == 0 || number == 4)
	{
		// TODO: is this the correct behavior?
		if((emu->cr[0] & X86_CR0_PG) != 0 && (emu->cr[4] & X86_CR4_PAE) == 0)
			value &= ~X86_EFER_LME;
	}
	if(number == 0)
	{
		// TODO: is this the correct behavior?
		if((emu->efer & X86_EFER_LME) != 0 && (emu->cr[0] & X86_CR0_PG) != 0)
			emu->efer |= X86_EFER_LMA;
		else
			emu->efer &= ~X86_EFER_LMA;
	}
}

// Debug registers

static inline uint32_t x86_debug_register_get32(x86_state_t * emu, int number)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	if((emu->dr[7] & X86_DR7_GD) != 0)
	{
		emu->dr[6] |= X86_DR6_BD;
		x86_trigger_interrupt(emu, X86_EXC_DB | X86_EXC_FAULT, 0);
	}
	if(number == 4 || number == 5)
	{
		if((emu->cr[4] & X86_CR4_DE) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		number += 2;
	}
	return emu->dr[number];
}

static inline uint64_t x86_debug_register_get64(x86_state_t * emu, int number)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	if((emu->dr[7] & X86_DR7_GD) != 0)
	{
		emu->dr[6] |= X86_DR6_BD;
		x86_trigger_interrupt(emu, X86_EXC_DB | X86_EXC_FAULT, 0);
	}
	if(number == 4 || number == 5)
	{
		if((emu->cr[4] & X86_CR4_DE) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		number += 2;
	}
	return emu->dr[number];
}

static inline void x86_debug_register_set32(x86_state_t * emu, int number, uint32_t value)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	switch(number)
	{
	case 4:
	case 5:
		if((emu->cr[4] & X86_CR4_DE) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		number += 2;
		break;
	case 6:
		if(emu->cpu_type <= X86_CPU_486)
			value &= 0x0000F00F;
		else
			value &= 0x0000E00F;
		break;
	case 7:
		if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
			value &= 0xFFFF23FF;
		else if(emu->cpu_type <= X86_CPU_486)
			value &= 0xFFFF13FF;
		else
			value &= 0xFFFF03FF;
		break;
	}
	if((emu->dr[7] & X86_DR7_GD) != 0)
	{
		emu->dr[6] |= X86_DR6_BD;
		x86_trigger_interrupt(emu, X86_EXC_DB | X86_EXC_FAULT, 0);
	}
	emu->dr[number] = value;
}

static inline void x86_debug_register_set64(x86_state_t * emu, int number, uint64_t value)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO
	// TODO: which flags are allowed, which CPU introduced it
	if(number == 4 || number == 5)
	{
		if((emu->cr[4] & X86_CR4_DE) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		number += 2;
	}
	if((emu->dr[7] & X86_DR7_GD) != 0)
	{
		emu->dr[6] |= X86_DR6_BD;
		x86_trigger_interrupt(emu, X86_EXC_BP | X86_EXC_FAULT, 0);
	}
	switch(number)
	{
	case 6:
	case 7:
		if((value & 0xFFFFFFFF00000000) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	}
	emu->dr[number] = value;
}

// Test registers

static inline bool x86_test_register_is_valid(x86_state_t * emu, int number)
{
	switch(emu->cpu_type)
	{
	case X86_CPU_386:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
			return false;

		return 6 <= number && number <= 7;

	case X86_CPU_486:
		return 2 <= number && number <= 7;

	case X86_CPU_CYRIX:
		if(emu->cpu_traits.cpu_subtype < X86_CPU_CYRIX_5X86)
		{
			return 2 <= number && number <= 7;
				x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		}
		else if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_GX2 || emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_LX)
		{
			return true;
		}
		else
		{
			// Note: unsure about Cyrix III, MediaGX, GXm, GX1
			return 1 <= number && number <= 7;
		}
	case X86_CPU_EXTENDED:
		return true;
	default:
		return false;
	}
}

static inline uint32_t x86_test_register_get32(x86_state_t * emu, int number)
{
	if(!x86_test_register_is_valid(emu, number))
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

	// TODO: what flags are allowed
	return emu->tr386[number];
}

// TODO: according to Wikipedia, 386 has undocumented TR4/TR5, Cyrix 5x86+ has TR1/TR2, Geode GX2 and AMD (not NatSemi) GX has all 8, WinChip behaves as NOP
static inline void x86_test_register_set32(x86_state_t * emu, int number, uint32_t value)
{
	if(!x86_test_register_is_valid(emu, number))
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

	// TODO: what happens on invalid flags
	switch(number)
	{
	case 3:
	case 5:
		break;
	case 4:
		value &= 0xFFFFFFF8;
		break;
	case 6:
		value &= 0xFFFFFFE1;
		break;
	case 7:
		if(emu->cpu_type == X86_CPU_386)
			value &= 0xFFFF001C;
		else
			value &= 0xFFFFFF9C;
		break;
	}
	emu->tr386[number] = value;
}

// Model specific registers

static inline uint64_t x86_cyrix_get_sel_msr(x86_state_t * emu, x86_segnum_t segment)
{
	return emu->sr[segment].selector | ((emu->sr[segment].access & 0x00F0FF00) << 4);
}

static inline void x86_cyrix_set_sel_msr(x86_state_t * emu, x86_segnum_t segment, uint64_t value)
{
	emu->sr[segment].selector = value;
	emu->sr[segment].access = (value >> 4) & 0x00F0FF00;
}

static inline uint64_t x86_cyrix_get_base_msr(x86_state_t * emu, x86_segnum_t segment)
{
	return emu->sr[segment].base | ((uint64_t)emu->sr[segment].access << 32);
}

static inline void x86_cyrix_set_base_msr(x86_state_t * emu, x86_segnum_t segment, uint64_t value)
{
	emu->sr[segment].base = value;
	emu->sr[segment].limit = value >> 32;
}

static inline uint64_t x86_cyrix_get_mr_msr(x86_state_t * emu, unsigned number)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		x87_convert_from_float80(emu->x87.fpr[number].f, &fraction, &exponent, &sign);
		return fraction;
	}
	else
	{
		return emu->x87.fpr[number].mmx.q[0];
	}
#else
	return emu->x87.fpr[number].fraction;
#endif
}

static inline void x86_cyrix_set_mr_msr(x86_state_t * emu, unsigned number, uint64_t value)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		x87_convert_from_float80(emu->x87.fpr[number].f, &fraction, &exponent, &sign);
		emu->x87.fpr[number].f = x87_convert_to_float80(value, exponent, sign);
	}
	else
	{
		emu->x87.fpr[number].mmx.q[0] = value;
	}
#else
	emu->x87.fpr[number].fraction = value;
#endif
}

static inline uint64_t x86_cyrix_get_er_msr(x86_state_t * emu, unsigned number)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		x87_convert_from_float80(emu->x87.fpr[number].f, &fraction, &exponent, &sign);
		return exponent | (sign ? 0x8000 : 0);
	}
	else
	{
		return emu->x87.fpr[number].exponent;
	}
#else
	return emu->x87.fpr[number].exponent;
#endif
}

static inline void x86_cyrix_set_er_msr(x86_state_t * emu, unsigned number, uint64_t value)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		x87_convert_from_float80(emu->x87.fpr[number].f, &fraction, &exponent, &sign);
		emu->x87.fpr[number].f = x87_convert_to_float80(fraction, value & 0x7FFF, (value & 0x8000) != 0);
	}
	else
	{
		emu->x87.fpr[number].exponent = value;
	}
#else
	emu->x87.fpr[number].exponent = value;
#endif
}

static inline bool x86_msr_is_valid(x86_state_t * emu, uint32_t index)
{
	switch(index)
	{
	// Intel
	case X86_R_MC_ADDR:
	case X86_R_MC_TYPE:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
		case X86_CPU_AMD:
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_TR1:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_id(&emu->cpu_traits) == (X86_ID_INTEL_PENTIUM >> 8);
		case X86_CPU_EXTENDED:
		case X86_CPU_WINCHIP:
			return true;
		default:
			return false;
		}

	case X86_R_MSR_TEST_DATA: // Cyrix
		switch(emu->cpu_type)
		{
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_id(&emu->cpu_traits) >= (X86_ID_CYRIX_6X86MX >> 8);
		default:
			return false;
		}

	case X86_R_TR2:
	//case X86_R_MSR_TEST_ADDRESS:
	case X86_R_TR3:
	//case X86_R_MSR_COMMAND_STATUS:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_id(&emu->cpu_traits) == (X86_ID_INTEL_PENTIUM >> 8);
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_id(&emu->cpu_traits) >= (X86_ID_CYRIX_6X86MX >> 8);
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_TR4:
	case X86_R_TR5:
	case X86_R_TR6:
	case X86_R_TR7:
	case X86_R_TR9:
	case X86_R_TR10:
	case X86_R_TR11:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_id(&emu->cpu_traits) == (X86_ID_INTEL_PENTIUM >> 8);
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_TR8:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) == X86_ID_INTEL_PENTIUM_STEPPING_A;
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_TR12:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_id(&emu->cpu_traits) == (X86_ID_INTEL_PENTIUM >> 8);
		case X86_CPU_WINCHIP:
		case X86_CPU_EXTENDED:
			return true;
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K6
				&& x86_cpuid_get_family_model_id(&emu->cpu_traits) < X86_ID_AMD_K7; // TODO: unknown if K7 supports it
		default:
			return false;
		}

	case X86_R_TSC:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
		case X86_CPU_AMD:
		case X86_CPU_WINCHIP:
		case X86_CPU_EXTENDED:
			return true;
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_GXM; // including X86_ID_CYRIX_6X86MX;
		default:
			return false;
		}

	case X86_R_CESR:
	case X86_R_CTR0:
	case X86_R_CTR1:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_id(&emu->cpu_traits) == (X86_ID_INTEL_PENTIUM >> 8);
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_id(&emu->cpu_traits) >= (X86_ID_CYRIX_6X86MX >> 8);
		case X86_CPU_WINCHIP:
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_APIC_BASE:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO;
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K8;
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_EBL_CR_POWERON:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO;
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K8;
		case X86_CPU_VIA:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_VIA_C3;
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_K5_AAR:
	case X86_R_K5_HWCR:
		switch(emu->cpu_type)
		{
		case X86_CPU_AMD:
			return X86_ID_AMD_K5 <= x86_cpuid_get_family_model_id(&emu->cpu_traits)
				&& x86_cpuid_get_family_model_id(&emu->cpu_traits) < X86_ID_AMD_K6;
		default:
			return false;
		}

	case X86_R_SMBASE:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return true; // TODO: check IA32_VMX_MISC[15]
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_PERFCTR0:
	case X86_R_PERFCTR1:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO; // TODO: check CPUID
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_GX2
				|| x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_LX;
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_FCR_WINCHIP:
	case X86_R_FCR1_WINCHIP:
	case X86_R_FCR2_WINCHIP:
		switch(emu->cpu_type)
		{
		case X86_CPU_WINCHIP:
			return true;
		default:
			return false;
		}

	case X86_R_FCR3_WINCHIP:
		switch(emu->cpu_type)
		{
		case X86_CPU_WINCHIP:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_WINCHIP2;
		default:
			return false;
		}

	case X86_R_MCR0:
	case X86_R_MCR1:
	case X86_R_MCR2:
	case X86_R_MCR3:
	case X86_R_MCR4:
	case X86_R_MCR5:
	case X86_R_MCR6:
	case X86_R_MCR7:
	case X86_R_MCR_CTRL:
		switch(emu->cpu_type)
		{
		case X86_CPU_WINCHIP:
			return true;
		default:
			return false;
		}

	case X86_R_BBL_CR_CTL3:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO;
		case X86_CPU_VIA:
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_SYSENTER_CS:
	case X86_R_SYSENTER_ESP:
	case X86_R_SYSENTER_EIP:
		return (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_SEP) != 0;

	case X86_R_MCG_CAP:
	case X86_R_MCG_STATUS:
	case X86_R_MCG_CTL: // TODO: check IA32_MCG_CAP[8]
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO;
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K8;
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	case X86_R_PERFEVTSEL0:
	case X86_R_PERFEVTSEL1:
		switch(emu->cpu_type)
		{
		case X86_CPU_INTEL:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_INTEL_PENTIUM_PRO; // TODO: check CPUID
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_GX2
				|| x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_LX;
		case X86_CPU_VIA:
		case X86_CPU_EXTENDED:
			return true;
		default:
			return false;
		}

	//case X86_R_DEBUGCTL:
		// TODO
	//case X86_R_LASTBRANCH_TOS:
		// TODO
	//case X86_R_LASTBRANCHFROMIP:
		// TODO
	//case X86_R_LASTBRANCHTOIP:
		// TODO
	//case X86_R_LER_FROM_IP:
		// TODO
	//case X86_R_LER_TO_IP:
		// TODO
	//case X86_R_LER_INFO:
		// TODO
	case X86_R_BNDCFGS:
		return (emu->cpu_traits.cpuid7_0.ebx & X86_CPUID7_0_EBX_MPX) != 0;
	case X86_R_FCR:
	case X86_R_FCR1:
	case X86_R_FCR2:
		switch(emu->cpu_type)
		{
		case X86_CPU_VIA:
			return true;
		default:
			return false;
		}
	//case X86_R_LONGHAUL:
		// TODO
	//case X86_R_RNG:
		// TODO

	// Cyrix
	case X86_R_GX2_PCR:
	case X86_R_SMM_CTL:
	case X86_R_DMI_CTL:
	case X86_R_GX2_ES_SEL:
	case X86_R_GX2_CS_SEL:
	case X86_R_GX2_SS_SEL:
	case X86_R_GX2_DS_SEL:
	case X86_R_GX2_FS_SEL:
	case X86_R_GX2_GS_SEL:
	case X86_R_GX2_LDT_SEL:
	case X86_R_GX2_TM_SEL:
	case X86_R_GX2_TSS_SEL:
	case X86_R_GX2_IDT_SEL:
	case X86_R_GX2_GDT_SEL:
	case X86_R_SMM_HDR:
	case X86_R_DMM_HDR:
	case X86_R_GX2_ES_BASE:
	case X86_R_GX2_CS_BASE:
	case X86_R_GX2_SS_BASE:
	case X86_R_GX2_DS_BASE:
	case X86_R_GX2_FS_BASE:
	case X86_R_GX2_GS_BASE:
	case X86_R_GX2_LDT_BASE:
	case X86_R_GX2_TM_BASE:
	case X86_R_GX2_TSS_BASE:
	case X86_R_GX2_IDT_BASE:
	case X86_R_GX2_GDT_BASE:
	case X86_R_SMM_BASE:
	case X86_R_DMM_BASE:
	case X86_R_GX2_DR1_DR0:
	case X86_R_GX2_DR3_DR2:
	case X86_R_GX2_DR7_DR6:
	case X86_R_GX2_FPENV_CS:
	case X86_R_GX2_FPENV_IP:
	case X86_R_GX2_FPENV_DS:
	case X86_R_GX2_FPENV_DP:
	case X86_R_GX2_FPENV_OP:
	case X86_R_GX2_GR_EAX:
	case X86_R_GX2_GR_ECX:
	case X86_R_GX2_GR_EDX:
	case X86_R_GX2_GR_EBX:
	case X86_R_GX2_GR_ESP:
	case X86_R_GX2_GR_EBP:
	case X86_R_GX2_GR_ESI:
	case X86_R_GX2_GR_EDI:
	case X86_R_GX2_EFLAG:
	case X86_R_GX2_CR0:
	case X86_R_GX2_CR1:
	case X86_R_GX2_CR2:
	case X86_R_GX2_CR3:
	case X86_R_GX2_CR4:
	case X86_R_GX2_FPU_CW:
	case X86_R_GX2_FPU_SW:
	case X86_R_GX2_FPU_TW:
	//case X86_R_GX2_FPU_BUSY: // TODO: unimplemented
	case X86_R_GX2_FPU_MAP:
	case X86_R_GX2_FPU_MR0:
	case X86_R_GX2_FPU_ER0:
	case X86_R_GX2_FPU_MR1:
	case X86_R_GX2_FPU_ER1:
	case X86_R_GX2_FPU_MR2:
	case X86_R_GX2_FPU_ER2:
	case X86_R_GX2_FPU_MR3:
	case X86_R_GX2_FPU_ER3:
	case X86_R_GX2_FPU_MR4:
	case X86_R_GX2_FPU_ER4:
	case X86_R_GX2_FPU_MR5:
	case X86_R_GX2_FPU_ER5:
	case X86_R_GX2_FPU_MR6:
	case X86_R_GX2_FPU_ER6:
	case X86_R_GX2_FPU_MR7:
	case X86_R_GX2_FPU_ER7:
	//case X86_R_GX2_FPU_MR8: // TODO: unimplemented
	//case X86_R_GX2_FPU_ER8:
	//case X86_R_GX2_FPU_MR9:
	//case X86_R_GX2_FPU_ER9:
	//case X86_R_GX2_FPU_MR10:
	//case X86_R_GX2_FPU_ER10:
	//case X86_R_GX2_FPU_MR11:
	//case X86_R_GX2_FPU_ER11:
	//case X86_R_GX2_FPU_MR12:
	//case X86_R_GX2_FPU_ER12:
	//case X86_R_GX2_FPU_MR13:
	//case X86_R_GX2_FPU_ER13:
	//case X86_R_GX2_FPU_MR14:
	//case X86_R_GX2_FPU_ER14:
	//case X86_R_GX2_FPU_MR15:
	//case X86_R_GX2_FPU_ER15:
		switch(emu->cpu_type)
		{
		case X86_CPU_CYRIX:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_GX2
				|| x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_CYRIX_LX;
		default:
			return false;
		}

	// AMD
	case X86_R_EFER:
		if(emu->cpu_type == X86_CPU_AMD)
		{
			return (emu->cpu_traits.cpuid_ext1.edx & (X86_CPUID_EXT1_EDX_SYSCALL_K6 | X86_CPUID_EXT1_EDX_SYSCALL)) != 0;
		}
		else
		{
			return (emu->cpu_traits.cpuid_ext1.edx & (X86_CPUID_EXT1_EDX_NX | X86_CPUID_EXT1_EDX_LM)) != 0;
		}
	case X86_R_STAR:
		if(emu->cpu_type == X86_CPU_AMD)
		{
			return (emu->cpu_traits.cpuid_ext1.edx & (X86_CPUID_EXT1_EDX_SYSCALL_K6 | X86_CPUID_EXT1_EDX_SYSCALL)) != 0;
		}
		else
		{
			return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
		}
	case X86_R_LSTAR:
	case X86_R_CSTAR:
	case X86_R_SF_MASK:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	case X86_R_UWCCR:
	case X86_R_PSOR:
	case X86_R_PFIR:
		switch(emu->cpu_type)
		{
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K6_2
				&& x86_cpuid_get_family_model_id(&emu->cpu_traits) < X86_ID_AMD_K7; // TODO: unknown if K7 supports it
		default:
			return false;
		}
	case X86_R_EPMR:
		switch(emu->cpu_type)
		{
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K6_III_PLUS
				&& x86_cpuid_get_family_model_id(&emu->cpu_traits) < X86_ID_AMD_K7; // TODO: unknown if K7 supports it
		default:
			return false;
		}
	case X86_R_L2AAR:
		switch(emu->cpu_type)
		{
		case X86_CPU_AMD:
			return x86_cpuid_get_family_model_id(&emu->cpu_traits) >= X86_ID_AMD_K6_III
				&& x86_cpuid_get_family_model_id(&emu->cpu_traits) < X86_ID_AMD_K7; // TODO: unknown if K7 supports it
		default:
			return false;
		}
	case X86_R_FS_BASE:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	case X86_R_GS_BASE:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	case X86_R_KERNEL_GS_BASE:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	//case X86_R_TSC_AUX:
		// TODO: check CPUID
	default:
		return false;
	}
}

static inline uint64_t x86_msr_get(x86_state_t * emu, uint32_t index)
{
	if(!x86_msr_is_valid(emu, index))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

	switch(index)
	{
	// Intel
	case X86_R_TR1:
		return emu->tr386[0]; // to accomodate hole between TR1 and TR2
	case X86_R_MSR_TEST_DATA: // Cyrix
		return emu->cyrix_test_data;
	case X86_R_TR2:
	//case X86_R_MSR_TEST_ADDRESS:
		if(emu->cpu_type == X86_CPU_CYRIX)
			return emu->cyrix_test_address;
		else
			return emu->tr386[2];
	case X86_R_TR3:
	//case X86_R_MSR_COMMAND_STATUS:
		if(emu->cpu_type == X86_CPU_CYRIX)
			return emu->cyrix_command_status;
		else
			return emu->tr386[3];
	case X86_R_TR4:
		return emu->tr386[4];
	case X86_R_TR5:
		return emu->tr386[5];
	case X86_R_TR6:
		return emu->tr386[6];
	case X86_R_TR7:
		return emu->tr386[7];
	case X86_R_TR8:
		return emu->tr386[8];
	case X86_R_TR9:
		return emu->tr386[9];
	case X86_R_TR10:
		return emu->tr386[10];
	case X86_R_TR11:
		return emu->tr386[11];
	case X86_R_TR12:
		return emu->tr386[12];
	case X86_R_TSC:
		return emu->tsc;
	case X86_R_CESR:
		return emu->msr_cesr;
	case X86_R_CTR0:
		return emu->msr_ctr0;
	case X86_R_CTR1:
		return emu->msr_ctr1;
	case X86_R_APIC_BASE:
		return emu->apic_base;
	case X86_R_EBL_CR_POWERON:
		return emu->msr_ebl_cr_poweron;
	case X86_R_K5_AAR:
		return emu->msr_k5_aar;
	case X86_R_K5_HWCR:
		return emu->msr_k5_hwcr;
	case X86_R_SMBASE:
		return emu->smbase;
	case X86_R_PERFCTR0:
		return emu->msr_perfctr0;
	case X86_R_PERFCTR1:
		return emu->msr_perfctr1;
	case X86_R_FCR_WINCHIP:
		return emu->msr_fcr;
	case X86_R_FCR1_WINCHIP:
		return emu->msr_fcr1;
	case X86_R_FCR2_WINCHIP:
		return emu->msr_fcr2;
	case X86_R_FCR3_WINCHIP:
		return emu->msr_fcr3;
	case X86_R_MCR0:
		return emu->msr_mcr[0];
	case X86_R_MCR1:
		return emu->msr_mcr[1];
	case X86_R_MCR2:
		return emu->msr_mcr[2];
	case X86_R_MCR3:
		return emu->msr_mcr[3];
	case X86_R_MCR4:
		return emu->msr_mcr[4];
	case X86_R_MCR5:
		return emu->msr_mcr[5];
	case X86_R_MCR6:
		return emu->msr_mcr[6];
	case X86_R_MCR7:
		return emu->msr_mcr[7];
	case X86_R_BBL_CR_CTL3:
		return emu->msr_bbl_cr_ctl3;
	case X86_R_MCR_CTRL:
		return emu->msr_mcr_ctrl;
	case X86_R_SYSENTER_CS:
		return emu->sysenter_cs;
	case X86_R_SYSENTER_ESP:
		return emu->sysenter_esp;
	case X86_R_SYSENTER_EIP:
		return emu->sysenter_eip;
	case X86_R_MCG_CAP:
		return emu->mcg_cap;
	case X86_R_MCG_STATUS:
		return emu->mcg_status;
	case X86_R_MCG_CTL:
		return emu->mcg_ctl;
	case X86_R_PERFEVTSEL0:
		return emu->perfevtsel0;
	case X86_R_PERFEVTSEL1:
		return emu->perfevtsel1;
	case X86_R_DEBUGCTL:
		return emu->debugctl;
	case X86_R_LASTBRANCH_TOS:
		return emu->msr_lastbranch_tos;
	case X86_R_LASTBRANCHFROMIP:
		return emu->msr_lastbranchfromip;
	case X86_R_LASTBRANCHTOIP:
		return emu->msr_lastbranchtoip;
	case X86_R_LER_FROM_IP:
		return emu->ler_from_ip;
	case X86_R_LER_TO_IP:
		return emu->ler_to_ip;
	case X86_R_LER_INFO:
		return emu->ler_info;
	case X86_R_BNDCFGS:
		return emu->bndcfgs;
	case X86_R_FCR:
		return emu->msr_fcr;
	case X86_R_FCR1:
		return emu->msr_fcr1;
	case X86_R_FCR2:
		return emu->msr_fcr2;
	case X86_R_LONGHAUL:
		return emu->msr_longhaul;
	case X86_R_RNG:
		return emu->msr_rng;

	// Cyrix
	case X86_R_GX2_PCR:
		return emu->gx2_pcr;
	case X86_R_SMM_CTL:
		return emu->smm_ctl;
	case X86_R_DMI_CTL:
		return emu->dmi_ctl;
	case X86_R_GX2_ES_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_ES);
	case X86_R_GX2_CS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_CS);
	case X86_R_GX2_SS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_SS);
	case X86_R_GX2_DS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_DS);
	case X86_R_GX2_FS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_FS);
	case X86_R_GX2_GS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_GS);
	case X86_R_GX2_LDT_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_LDTR);
	case X86_R_GX2_TM_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_TM_SEG);
	case X86_R_GX2_TSS_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_TR);
	case X86_R_GX2_IDT_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_IDTR);
	case X86_R_GX2_GDT_SEL:
		return x86_cyrix_get_sel_msr(emu, X86_R_GDTR);
	case X86_R_SMM_HDR:
		return emu->smm_hdr;
	case X86_R_DMM_HDR:
		return emu->dmm_hdr;
	case X86_R_GX2_ES_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_ES);
	case X86_R_GX2_CS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_CS);
	case X86_R_GX2_SS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_SS);
	case X86_R_GX2_DS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_DS);
	case X86_R_GX2_FS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_FS);
	case X86_R_GX2_GS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_GS);
	case X86_R_GX2_LDT_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_LDTR);
	case X86_R_GX2_TM_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_TM_SEG);
	case X86_R_GX2_TSS_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_TR);
	case X86_R_GX2_IDT_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_IDTR);
	case X86_R_GX2_GDT_BASE:
		return x86_cyrix_get_base_msr(emu, X86_R_GDTR);
	case X86_R_SMM_BASE:
		return emu->smm.base | ((uint64_t)emu->smm.limit << 32);
	case X86_R_DMM_BASE:
		return emu->dmm.base | ((uint64_t)emu->dmm.limit << 32);
	case X86_R_GX2_DR1_DR0:
		return emu->dr[0] | ((uint64_t)emu->dr[1] << 32);
	case X86_R_GX2_DR3_DR2:
		return emu->dr[2] | ((uint64_t)emu->dr[3] << 32);
	case X86_R_GX2_DR7_DR6:
		return emu->dr[6] | ((uint64_t)emu->dr[7] << 32);
	case X86_R_GX2_FPENV_CS:
		return emu->x87.fcs;
	case X86_R_GX2_FPENV_IP:
		return emu->x87.fip;
	case X86_R_GX2_FPENV_DS:
		return emu->x87.fds;
	case X86_R_GX2_FPENV_DP:
		return emu->x87.fdp;
	case X86_R_GX2_FPENV_OP:
		return emu->x87.fop;
	case X86_R_GX2_GR_EAX:
		return emu->gpr[X86_R_AX];
	case X86_R_GX2_GR_ECX:
		return emu->gpr[X86_R_CX];
	case X86_R_GX2_GR_EDX:
		return emu->gpr[X86_R_DX];
	case X86_R_GX2_GR_EBX:
		return emu->gpr[X86_R_BX];
	case X86_R_GX2_GR_ESP:
		return emu->gpr[X86_R_SP];
	case X86_R_GX2_GR_EBP:
		return emu->gpr[X86_R_BP];
	case X86_R_GX2_GR_ESI:
		return emu->gpr[X86_R_SI];
	case X86_R_GX2_GR_EDI:
		return emu->gpr[X86_R_DI];
	case X86_R_GX2_EFLAG:
		return x86_flags_get32(emu);
	case X86_R_GX2_CR0:
		return emu->cr[0];
	case X86_R_GX2_CR1:
		return emu->cr[1];
	case X86_R_GX2_CR2:
		return emu->cr[2];
	case X86_R_GX2_CR3:
		return emu->cr[3];
	case X86_R_GX2_CR4:
		return emu->cr[4];
	case X86_R_GX2_FPU_CW:
		return emu->x87.cw;
	case X86_R_GX2_FPU_SW:
		return emu->x87.sw;
	case X86_R_GX2_FPU_TW:
		return emu->x87.tw;
	//case X86_R_GX2_FPU_BUSY: // TODO
	case X86_R_GX2_FPU_MAP:
		return 0x0000000076543210;
	case X86_R_GX2_FPU_MR0:
		return x86_cyrix_get_mr_msr(emu, 0);
	case X86_R_GX2_FPU_ER0:
		return x86_cyrix_get_er_msr(emu, 0);
	case X86_R_GX2_FPU_MR1:
		return x86_cyrix_get_mr_msr(emu, 1);
	case X86_R_GX2_FPU_ER1:
		return x86_cyrix_get_er_msr(emu, 1);
	case X86_R_GX2_FPU_MR2:
		return x86_cyrix_get_mr_msr(emu, 2);
	case X86_R_GX2_FPU_ER2:
		return x86_cyrix_get_er_msr(emu, 2);
	case X86_R_GX2_FPU_MR3:
		return x86_cyrix_get_mr_msr(emu, 3);
	case X86_R_GX2_FPU_ER3:
		return x86_cyrix_get_er_msr(emu, 3);
	case X86_R_GX2_FPU_MR4:
		return x86_cyrix_get_mr_msr(emu, 4);
	case X86_R_GX2_FPU_ER4:
		return x86_cyrix_get_er_msr(emu, 4);
	case X86_R_GX2_FPU_MR5:
		return x86_cyrix_get_mr_msr(emu, 5);
	case X86_R_GX2_FPU_ER5:
		return x86_cyrix_get_er_msr(emu, 5);
	case X86_R_GX2_FPU_MR6:
		return x86_cyrix_get_mr_msr(emu, 6);
	case X86_R_GX2_FPU_ER6:
		return x86_cyrix_get_er_msr(emu, 6);
	case X86_R_GX2_FPU_MR7:
		return x86_cyrix_get_mr_msr(emu, 7);
	case X86_R_GX2_FPU_ER7:
		return x86_cyrix_get_er_msr(emu, 7);
	//case X86_R_GX2_FPU_MR8: // TODO: unimplemented
	//case X86_R_GX2_FPU_ER8:
	//case X86_R_GX2_FPU_MR9:
	//case X86_R_GX2_FPU_ER9:
	//case X86_R_GX2_FPU_MR10:
	//case X86_R_GX2_FPU_ER10:
	//case X86_R_GX2_FPU_MR11:
	//case X86_R_GX2_FPU_ER11:
	//case X86_R_GX2_FPU_MR12:
	//case X86_R_GX2_FPU_ER12:
	//case X86_R_GX2_FPU_MR13:
	//case X86_R_GX2_FPU_ER13:
	//case X86_R_GX2_FPU_MR14:
	//case X86_R_GX2_FPU_ER14:
	//case X86_R_GX2_FPU_MR15:
	//case X86_R_GX2_FPU_ER15:

	// AMD
	case X86_R_EFER:
		return emu->efer;
	case X86_R_STAR:
		return emu->star;
	case X86_R_LSTAR:
		return emu->lstar;
	case X86_R_CSTAR:
		return emu->cstar;
	case X86_R_SF_MASK:
		return emu->sf_mask;
	case X86_R_UWCCR:
		return emu->msr_uwccr;
	case X86_R_PSOR:
		return emu->msr_psor;
	case X86_R_PFIR:
		return emu->msr_pfir;
	case X86_R_EPMR:
		return emu->msr_epmr;
	case X86_R_L2AAR:
		return emu->msr_l2aar;
	case X86_R_FS_BASE:
		return emu->sr[X86_R_FS].base;
	case X86_R_GS_BASE:
		return emu->sr[X86_R_GS].base;
	case X86_R_KERNEL_GS_BASE:
		return emu->kernel_gs_base;
	case X86_R_TSC_AUX:
		return emu->tsc_aux;

	default:
		assert(false);
	}
}

static inline void x86_msr_set(x86_state_t * emu, uint32_t index, uint64_t value)
{
	if(!x86_msr_is_valid(emu, index))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

	switch(index)
	{
	// Intel
	case X86_R_TR1:
		emu->tr386[0] = value; // to accomodate hole between TR1 and TR2
		break;
	case X86_R_MSR_TEST_DATA: // Cyrix
		emu->cyrix_test_data = value;
		break;
	case X86_R_TR2:
	//case X86_R_MSR_TEST_ADDRESS:
		if(emu->cpu_type == X86_CPU_CYRIX)
			emu->cyrix_test_address = value;
		else
			emu->tr386[2] = value;
		break;
	case X86_R_TR3:
	//case X86_R_MSR_COMMAND_STATUS:
		if(emu->cpu_type == X86_CPU_CYRIX)
			emu->cyrix_command_status = value;
		else
			emu->tr386[3] = value;
		break;
	case X86_R_TR4:
		emu->tr386[4] = value;
		break;
	case X86_R_TR5:
		emu->tr386[5] = value;
		break;
	case X86_R_TR6:
		emu->tr386[6] = value;
		break;
	case X86_R_TR7:
		emu->tr386[7] = value;
		break;
	case X86_R_TR8:
		emu->tr386[8] = value;
		break;
	case X86_R_TR9:
		emu->tr386[9] = value;
		break;
	case X86_R_TR10:
		emu->tr386[10] = value;
		break;
	case X86_R_TR11:
		emu->tr386[11] = value;
		break;
	case X86_R_TR12:
		emu->tr386[12] = value;
		break;
	case X86_R_TSC:
		emu->tsc = value;
		break;

	case X86_R_CESR:
		// TODO
		emu->msr_cesr = value;
		break;
	case X86_R_CTR0:
		// TODO
		emu->msr_ctr0 = value;
		break;
	case X86_R_CTR1:
		// TODO
		emu->msr_ctr1 = value;
		break;
	case X86_R_APIC_BASE:
		// TODO
		emu->apic_base = value;
		break;
	case X86_R_EBL_CR_POWERON:
		// TODO
		emu->msr_ebl_cr_poweron = value;
		break;
	case X86_R_K5_AAR:
		// TODO
		emu->msr_k5_aar = value;
		break;
	case X86_R_K5_HWCR:
		// TODO
		emu->msr_k5_hwcr = value;
		break;
	case X86_R_SMBASE:
		// TODO
		emu->smbase = value;
		break;
	case X86_R_PERFCTR0:
		// TODO
		emu->msr_perfctr0 = value;
		break;
	case X86_R_PERFCTR1:
		// TODO
		emu->msr_perfctr1 = value;
		break;
	case X86_R_FCR_WINCHIP:
		// TODO
		emu->msr_fcr = value;
		break;
	case X86_R_FCR1_WINCHIP:
		// TODO
		emu->msr_fcr1 = value;
		break;
	case X86_R_FCR2_WINCHIP:
		// TODO
		emu->msr_fcr2 = value;
		break;
	case X86_R_FCR3_WINCHIP:
		// TODO
		emu->msr_fcr3 = value;
		break;

	case X86_R_MCR0:
		// TODO
		emu->msr_mcr[0] = value;
		break;
	case X86_R_MCR1:
		// TODO
		emu->msr_mcr[1] = value;
		break;
	case X86_R_MCR2:
		// TODO
		emu->msr_mcr[2] = value;
		break;
	case X86_R_MCR3:
		// TODO
		emu->msr_mcr[3] = value;
		break;
	case X86_R_MCR4:
		// TODO
		emu->msr_mcr[4] = value;
		break;
	case X86_R_MCR5:
		// TODO
		emu->msr_mcr[5] = value;
		break;
	case X86_R_MCR6:
		// TODO
		emu->msr_mcr[6] = value;
		break;
	case X86_R_MCR7:
		// TODO
		emu->msr_mcr[7] = value;
		break;
	case X86_R_BBL_CR_CTL3:
		// TODO
		emu->msr_bbl_cr_ctl3 = value;
		break;
	case X86_R_MCR_CTRL:
		// TODO
		emu->msr_mcr_ctrl = value;
		break;
	case X86_R_SYSENTER_CS:
		emu->sysenter_cs = value = value;
		break;
	case X86_R_SYSENTER_ESP:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sysenter_esp = value;
		break;
	case X86_R_SYSENTER_EIP:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sysenter_eip = value;
		break;
	case X86_R_MCG_CAP:
		// TODO
		emu->mcg_cap = value;
		break;
	case X86_R_MCG_STATUS:
		// TODO
		emu->mcg_status = value;
		break;
	case X86_R_MCG_CTL:
		// TODO
		emu->mcg_ctl = value;
		break;
	case X86_R_PERFEVTSEL0:
		// TODO
		emu->perfevtsel0 = value;
		break;
	case X86_R_PERFEVTSEL1:
		// TODO
		emu->perfevtsel1 = value;
		break;
	case X86_R_DEBUGCTL:
		// TODO
		emu->debugctl = value;
		break;
	case X86_R_LASTBRANCH_TOS:
		// TODO
		emu->msr_lastbranch_tos = value;
		break;
	case X86_R_LASTBRANCHFROMIP:
		// TODO
		emu->msr_lastbranchfromip = value;
		break;
	case X86_R_LASTBRANCHTOIP:
		// TODO
		emu->msr_lastbranchtoip = value;
		break;
	case X86_R_LER_FROM_IP:
		// TODO
		emu->ler_from_ip = value;
		break;
	case X86_R_LER_TO_IP:
		// TODO
		emu->ler_to_ip = value;
		break;
	case X86_R_LER_INFO:
		// TODO
		emu->ler_info = value;
		break;
	case X86_R_BNDCFGS:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->bndcfgs = value;
		break;
	case X86_R_FCR:
		// TODO
		emu->msr_fcr = value;
		break;
	case X86_R_FCR1:
		// TODO
		emu->msr_fcr1 = value;
		break;
	case X86_R_FCR2:
		// TODO
		emu->msr_fcr2 = value;
		break;
	case X86_R_LONGHAUL:
		// TODO
		emu->msr_longhaul = value;
		break;
	case X86_R_RNG:
		// TODO
		emu->msr_rng = value;
		break;

	// Cyrix
		break;
	case X86_R_GX2_PCR:
		emu->gx2_pcr = value;
		break;
	case X86_R_SMM_CTL:
		emu->smm_ctl = value;
		break;
	case X86_R_DMI_CTL:
		emu->dmi_ctl = value;
		break;
	case X86_R_GX2_ES_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_ES, value);
		break;
	case X86_R_GX2_CS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_CS, value);
		break;
	case X86_R_GX2_SS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_SS, value);
		break;
	case X86_R_GX2_DS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_DS, value);
		break;
	case X86_R_GX2_FS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_FS, value);
		break;
	case X86_R_GX2_GS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_GS, value);
		break;
	case X86_R_GX2_LDT_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_LDTR, value);
		break;
	case X86_R_GX2_TM_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_TM_SEG, value);
		break;
	case X86_R_GX2_TSS_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_TR, value);
		break;
	case X86_R_GX2_IDT_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_IDTR, value);
		break;
	case X86_R_GX2_GDT_SEL:
		x86_cyrix_set_sel_msr(emu, X86_R_GDTR, value);
		break;
	case X86_R_SMM_HDR:
		emu->smm_hdr = value;
		break;
	case X86_R_DMM_HDR:
		emu->dmm_hdr = value;
		break;
	case X86_R_GX2_ES_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_ES, value);
		break;
	case X86_R_GX2_CS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_CS, value);
		break;
	case X86_R_GX2_SS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_SS, value);
		break;
	case X86_R_GX2_DS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_DS, value);
		break;
	case X86_R_GX2_FS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_FS, value);
		break;
	case X86_R_GX2_GS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_GS, value);
		break;
	case X86_R_GX2_LDT_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_LDTR, value);
		break;
	case X86_R_GX2_TM_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_TM_SEG, value);
		break;
	case X86_R_GX2_TSS_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_TR, value);
		break;
	case X86_R_GX2_IDT_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_IDTR, value);
		break;
	case X86_R_GX2_GDT_BASE:
		x86_cyrix_set_base_msr(emu, X86_R_GDTR, value);
		break;
	case X86_R_SMM_BASE:
		emu->smm.base = value;
		emu->smm.limit = value >> 32;
		break;
	case X86_R_DMM_BASE:
		emu->dmm.base = value;
		emu->dmm.limit = value >> 32;
		break;
	case X86_R_GX2_DR1_DR0:
		emu->dr[0] = value;
		emu->dr[1] = value >> 32;
		break;
	case X86_R_GX2_DR3_DR2:
		emu->dr[2] = value;
		emu->dr[3] = value >> 32;
		break;
	case X86_R_GX2_DR7_DR6:
		emu->dr[6] = value;
		emu->dr[7] = value >> 32;
		break;
	case X86_R_GX2_FPENV_CS:
		emu->x87.fcs = value;
		break;
	case X86_R_GX2_FPENV_IP:
		emu->x87.fip = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_FPENV_DS:
		emu->x87.fds = value;
		break;
	case X86_R_GX2_FPENV_DP:
		emu->x87.fdp = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_FPENV_OP:
		emu->x87.fop = 0xB800 | (value & 0x07FF);
		break;
	case X86_R_GX2_GR_EAX:
		emu->gpr[X86_R_AX] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_ECX:
		emu->gpr[X86_R_CX] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_EDX:
		emu->gpr[X86_R_DX] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_EBX:
		emu->gpr[X86_R_BX] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_ESP:
		emu->gpr[X86_R_SP] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_EBP:
		emu->gpr[X86_R_BP] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_ESI:
		emu->gpr[X86_R_SI] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_GR_EDI:
		emu->gpr[X86_R_DI] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_EFLAG:
		x86_flags_set32(emu, value);
		break;
	case X86_R_GX2_CR0:
		emu->cr[0] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_CR1:
		emu->cr[1] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_CR2:
		emu->cr[2] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_CR3:
		emu->cr[3] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_CR4:
		emu->cr[4] = value & 0xFFFFFFFF;
		break;
	case X86_R_GX2_FPU_CW:
		emu->x87.cw = value;
		break;
	case X86_R_GX2_FPU_SW:
		emu->x87.sw = value;
		break;
	case X86_R_GX2_FPU_TW:
		emu->x87.tw = value;
		break;
	//case X86_R_GX2_FPU_BUSY: // TODO
	case X86_R_GX2_FPU_MAP:
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
		break;
	case X86_R_GX2_FPU_MR0:
		x86_cyrix_set_mr_msr(emu, 0, value);
		break;
	case X86_R_GX2_FPU_ER0:
		x86_cyrix_set_er_msr(emu, 0, value);
		break;
	case X86_R_GX2_FPU_MR1:
		x86_cyrix_set_mr_msr(emu, 1, value);
		break;
	case X86_R_GX2_FPU_ER1:
		x86_cyrix_set_er_msr(emu, 1, value);
		break;
	case X86_R_GX2_FPU_MR2:
		x86_cyrix_set_mr_msr(emu, 2, value);
		break;
	case X86_R_GX2_FPU_ER2:
		x86_cyrix_set_er_msr(emu, 2, value);
		break;
	case X86_R_GX2_FPU_MR3:
		x86_cyrix_set_mr_msr(emu, 3, value);
		break;
	case X86_R_GX2_FPU_ER3:
		x86_cyrix_set_er_msr(emu, 3, value);
		break;
	case X86_R_GX2_FPU_MR4:
		x86_cyrix_set_mr_msr(emu, 4, value);
		break;
	case X86_R_GX2_FPU_ER4:
		x86_cyrix_set_er_msr(emu, 4, value);
		break;
	case X86_R_GX2_FPU_MR5:
		x86_cyrix_set_mr_msr(emu, 5, value);
		break;
	case X86_R_GX2_FPU_ER5:
		x86_cyrix_set_er_msr(emu, 5, value);
		break;
	case X86_R_GX2_FPU_MR6:
		x86_cyrix_set_mr_msr(emu, 6, value);
		break;
	case X86_R_GX2_FPU_ER6:
		x86_cyrix_set_er_msr(emu, 6, value);
		break;
	case X86_R_GX2_FPU_MR7:
		x86_cyrix_set_mr_msr(emu, 7, value);
		break;
	case X86_R_GX2_FPU_ER7:
		x86_cyrix_set_er_msr(emu, 7, value);
		break;
	//case X86_R_GX2_FPU_MR8: // TODO: unimplemented
	//case X86_R_GX2_FPU_ER8:
	//case X86_R_GX2_FPU_MR9:
	//case X86_R_GX2_FPU_ER9:
	//case X86_R_GX2_FPU_MR10:
	//case X86_R_GX2_FPU_ER10:
	//case X86_R_GX2_FPU_MR11:
	//case X86_R_GX2_FPU_ER11:
	//case X86_R_GX2_FPU_MR12:
	//case X86_R_GX2_FPU_ER12:
	//case X86_R_GX2_FPU_MR13:
	//case X86_R_GX2_FPU_ER13:
	//case X86_R_GX2_FPU_MR14:
	//case X86_R_GX2_FPU_ER14:
	//case X86_R_GX2_FPU_MR15:
	//case X86_R_GX2_FPU_ER15:

	// AMD
		break;
	case X86_R_EFER:
		if((emu->cr[0] & X86_CR0_PG) != 0 && (value & X86_EFER_LME) != (emu->efer & X86_EFER_LME))
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

		if((value & X86_EFER_LME) != 0 && (emu->cr[0] & X86_CR0_PG) != 0)
			value |= X86_EFER_LMA;
		else
			value &= ~X86_EFER_LMA;

		/* TODO: other bits? */

		emu->efer = value;
		break;
	case X86_R_STAR:
		emu->star = value;
		break;
	case X86_R_LSTAR:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->lstar = value;
		break;
	case X86_R_CSTAR:
		emu->cstar = value;
		break;
	case X86_R_SF_MASK:
		emu->sf_mask = value;
		break;
	case X86_R_UWCCR:
		// TODO
		emu->msr_uwccr = value;
		break;
	case X86_R_PSOR:
		// TODO
		emu->msr_psor = value;
		break;
	case X86_R_PFIR:
		// TODO
		emu->msr_pfir = value;
		break;
	case X86_R_EPMR:
		// TODO
		emu->msr_epmr = value;
		break;
	case X86_R_L2AAR:
		// TODO
		emu->msr_l2aar = value;
		break;
	case X86_R_FS_BASE:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sr[X86_R_FS].base = value;
		break;
	case X86_R_GS_BASE:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sr[X86_R_GS].base = value;
		break;
	case X86_R_KERNEL_GS_BASE:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->kernel_gs_base = value;
		break;
	case X86_R_TSC_AUX:
		// TODO
		emu->tsc_aux = value;
		break;
	}
}

// Cyrix configuration control registers

static inline bool x86_cyrix_is_register(x86_state_t * emu, uint8_t number, bool write)
{
	switch(number)
	{
	case 0x20: // PCR0 or TWR0
		return (X86_CPU_CYRIX_5X86 <= emu->cpu_traits.cpu_subtype && emu->cpu_traits.cpu_subtype <= X86_CPU_CYRIX_GX1) || (emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x10);
	case 0x41: // LCR1
	case 0x48: // BCR1
	case 0x49: // BCR2
		return emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x40;
	case 0xA4: // ARR4
	case 0xA5:
	case 0xA6:
	case 0xA7: // ARR5
	case 0xA8:
	case 0xA9:
	case 0xAA: // ARR6
	case 0xAB:
	case 0xAC:
	case 0xAD: // ARR7
	case 0xAE:
	case 0xAF:
		return emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x60) == 0x20 && (write || (emu->ccr[3] & 0x10) != 0);
	case 0xB0: // SMHR
	case 0xB1:
	case 0xB2:
	case 0xB3:
	case 0xB8: // GCR
	case 0xB9: // VGACTL
	case 0xBA: // VGAM
	case 0xBB:
	case 0xBC:
	case 0xBD:
		return X86_CPU_CYRIX_MEDIAGX <= emu->cpu_traits.cpu_subtype && emu->cpu_traits.cpu_subtype <= X86_CPU_CYRIX_GX1;
	case 0xC0: // CCR0
	case 0xC5: // ARR0 (486SLC: NCR1, 486SLC/e: ARR1)
	case 0xC6:
	case 0xC8: // ARR1 (486SLC: NCR1, 486SLC/e: ARR1)
	case 0xC9:
	case 0xCB: // ARR2 (486SLC: NCR1, 486SLC/e: ARR1)
	case 0xCC:
		return emu->cpu_traits.cpu_subtype <= X86_CPU_CYRIX_CX486SLCE || X86_CPU_CYRIX_6X86 <= emu->cpu_traits.cpu_subtype;
	case 0xC1: // CCR1
	case 0xCE: // ARR3 (486SLC: NCR4, 486SLC/e: ARR4, 5x86/MediaGX: SMAR)
	case 0xCF:
		return true;
	case 0xC2: // CCR2
	case 0xC3: // CCR3
	case 0xCD: // ARR3 (5x86/MediaGX: SMAR)
		return X86_CPU_CYRIX_5X86 <= emu->cpu_traits.cpu_subtype;
	case 0xE8: // CCR4
		return X86_CPU_CYRIX_5X86 <= emu->cpu_traits.cpu_subtype && (emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x10);
	case 0xE9: // CCR5
		return X86_CPU_CYRIX_6X86 <= emu->cpu_traits.cpu_subtype && (emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x10);
	case 0xC4: // CCR4
	case 0xC7: // ARR0
	case 0xCA: // ARR1
		return X86_CPU_CYRIX_6X86 <= emu->cpu_traits.cpu_subtype;
	case 0xD3: // ARR5
	case 0xD4:
	case 0xD5:
	case 0xD9: // ARR7
	case 0xDA:
	case 0xDB:
	case 0xD0: // ARR4 (Cyrix III: also ARR12)
	case 0xD1:
	case 0xD2:
	case 0xD6: // ARR5 (Cyrix III: also ARR13)
	case 0xD7:
	case 0xD8:
		return X86_CPU_CYRIX_6X86 <= emu->cpu_traits.cpu_subtype && (emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_III || (emu->ccr[3] & 0xF0) == 0x10 || (emu->ccr[3] & 0x60) == 0x20);
	case 0xDC: // RCR0 (Cyrix III: also RCR8)
	case 0xDD: // RCR1 (Cyrix III: also RCR9)
	case 0xDE: // RCR2 (Cyrix III: also RCR10)
	case 0xDF: // RCR3 (Cyrix III: also RCR11)
	case 0xE0: // RCR4 (Cyrix III: also RCR12)
	case 0xE1: // RCR5 (Cyrix III: also RCR13)
	case 0xE2: // RCR6
	case 0xE3: // RCR7
		return X86_CPU_CYRIX_6X86 <= emu->cpu_traits.cpu_subtype && (emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_III && ((emu->ccr[3] & 0xF0) == 0x10 || (emu->ccr[3] & 0x60) == 0x20));
	case 0xEA: // CCR6
		return X86_CPU_CYRIX_M2 <= emu->cpu_traits.cpu_subtype && (emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x10);
	case 0xEB: // CCR7
		return (X86_CPU_CYRIX_5X86 <= emu->cpu_traits.cpu_subtype && emu->cpu_traits.cpu_subtype <= X86_CPU_CYRIX_GX1) || (emu->ccr[3] & 0xF0) == 0x10;
	case 0xF0: // 5x86: PMR, GX1: PCR1
		return emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_5X86 || emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_GX1;
	case 0xFB: // DIR2
	case 0xFC: // DIR3
	case 0xFD: // DIR4
		return emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0xF0) == 0x10;
	case 0xFE: // DIR0
	case 0xFF: // DIR0
		return X86_CPU_CYRIX_5X86 <= emu->cpu_traits.cpu_subtype && emu->cpu_traits.cpu_subtype != X86_CPU_CYRIX_M2;
	default:
		return false;
	}
}

static inline uint8_t x86_cyrix_register_get(x86_state_t * emu, uint8_t number)
{
	if(!x86_cyrix_is_register(emu, number, true))
		return 0;

	switch(number)
	{
	case 0x20:
		return emu->pcr[0];
	case 0x41:
		return emu->lcr1;
	case 0x48:
		return emu->bcr[0];
	case 0x49:
		return emu->bcr[1];
	case 0xA4:
		return emu->arr[8] >> 16;
	case 0xA5:
		return emu->arr[8] >> 8;
	case 0xA6:
		return emu->arr[8];
	case 0xA7:
		return emu->arr[9] >> 16;
	case 0xA8:
		return emu->arr[9] >> 8;
	case 0xA9:
		return emu->arr[9];
	case 0xAA:
		return emu->arr[10] >> 16;
	case 0xAB:
		return emu->arr[10] >> 8;
	case 0xAC:
		return emu->arr[10];
	case 0xAD:
		return emu->arr[11] >> 16;
	case 0xAE:
		return emu->arr[11] >> 8;
	case 0xAF:
		return emu->arr[11];
	case 0xB0:
		return emu->smm_hdr;
	case 0xB1:
		return emu->smm_hdr >> 8;
	case 0xB2:
		return emu->smm_hdr >> 16;
	case 0xB3:
		return emu->smm_hdr >> 24;
	case 0xB8:
		return emu->gcr;
	case 0xB9:
		return emu->vgactl;
	case 0xBA:
		return emu->vgam;
	case 0xBB:
		return emu->vgam >> 8;
	case 0xBC:
		return emu->vgam >> 16;
	case 0xBD:
		return emu->vgam >> 24;
	case 0xC0:
		return emu->ccr[0];
	case 0xC1:
		return emu->ccr[1];
	case 0xC2:
		return emu->ccr[2];
	case 0xC3:
		return emu->ccr[3];
	case 0xC4:
		return emu->arr[0] >> 16;
	case 0xC5:
		return emu->arr[0] >> 8;
	case 0xC6:
		return emu->arr[0];
	case 0xC7:
		return emu->arr[1] >> 16;
	case 0xC8:
		return emu->arr[1] >> 8;
	case 0xC9:
		return emu->arr[1];
	case 0xCA:
		return emu->arr[2] >> 16;
	case 0xCB:
		return emu->arr[2] >> 8;
	case 0xCC:
		return emu->arr[2];
	case 0xCD:
		return emu->arr[3] >> 16;
	case 0xCE:
		return emu->arr[3] >> 8;
	case 0xCF:
		return emu->arr[3];
	case 0xD0:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[12] >> 16;
		else
			return emu->arr[4] >> 16;
	case 0xD1:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[12] >> 8;
		else
			return emu->arr[4] >> 8;
	case 0xD2:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[12];
		else
			return emu->arr[4];
	case 0xD3:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[13] >> 16;
		else
			return emu->arr[5] >> 16;
	case 0xD4:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[13] >> 8;
		else
			return emu->arr[5] >> 8;
	case 0xD5:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->arr[13];
		else
			return emu->arr[5];
	case 0xD6:
		return emu->arr[6] >> 16;
	case 0xD7:
		return emu->arr[6] >> 8;
	case 0xD8:
		return emu->arr[6];
	case 0xD9:
		return emu->arr[7] >> 16;
	case 0xDA:
		return emu->arr[7] >> 8;
	case 0xDB:
		return emu->arr[7];
	case 0xDC:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[8];
		else
			return emu->rcr[0];
	case 0xDD:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[9];
		else
			return emu->rcr[1];
	case 0xDE:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[10];
		else
			return emu->rcr[2];
	case 0xDF:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[11];
		else
			return emu->rcr[3];
	case 0xE0:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[12];
		else
			return emu->rcr[4];
	case 0xE1:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[13];
		else
			return emu->rcr[5];
	case 0xE2:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[14];
		else
			return emu->rcr[6];
	case 0xE3:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			return emu->rcr[15];
		else
			return emu->rcr[7];
	case 0xE8:
		// 5x86, MediaGX, 6x86, M2
		return emu->ccr[4];
	case 0xE9:
		// 5x86, MediaGX, 6x86, M2
		return emu->ccr[5];
	case 0xEA:
		// M2
		return emu->ccr[6];
	case 0xEB:
		// 5x86, MediaGX
		return emu->ccr[7];
	case 0xF0:
		return emu->pcr[1];
	case 0xFB:
		return emu->dir[4];
	case 0xFC:
		return emu->dir[3];
	case 0xFD:
		return emu->dir[2];
	case 0xFE:
		return emu->dir[0];
	case 0xFF:
		return emu->dir[1];
	default:
		return 0;
	}
}

static inline void x86_cyrix_register_set(x86_state_t * emu, uint8_t number, uint8_t value)
{
	if(!x86_cyrix_is_register(emu, number, false))
		return;

	switch(number)
	{
	case 0x20:
		emu->pcr[0] = value;
		break;
	case 0x41:
		emu->lcr1 = value;
		break;
	case 0x48:
		emu->bcr[0] = value;
		break;
	case 0x49:
		emu->bcr[1] = value;
		break;
	case 0xA4:
		emu->arr[8] = (emu->arr[8] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xA2:
		emu->arr[8] = (emu->arr[8] & ~0xFF00) | (value << 8);
		break;
	case 0xA3:
		emu->arr[8] = (emu->arr[8] & ~0xFF) | value;
		break;
	case 0xA7:
		emu->arr[9] = (emu->arr[9] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xA8:
		emu->arr[9] = (emu->arr[9] & ~0xFF00) | (value << 8);
		break;
	case 0xA9:
		emu->arr[9] = (emu->arr[9] & ~0xFF) | value;
		break;
	case 0xAA:
		emu->arr[10] = (emu->arr[10] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xAB:
		emu->arr[10] = (emu->arr[10] & ~0xFF00) | (value << 8);
		break;
	case 0xAC:
		emu->arr[10] = (emu->arr[10] & ~0xFF) | value;
		break;
	case 0xAD:
		emu->arr[11] = (emu->arr[11] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xAE:
		emu->arr[11] = (emu->arr[11] & ~0xFF00) | (value << 8);
		break;
	case 0xAF:
		emu->arr[11] = (emu->arr[11] & ~0xFF) | value;
		break;
	case 0xB0:
		emu->smm_hdr = (emu->smm_hdr & ~0xFF) | value;
		break;
	case 0xB1:
		emu->smm_hdr = (emu->smm_hdr & ~0xFF00) | (value << 8);
		break;
	case 0xB2:
		emu->smm_hdr = (emu->smm_hdr & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xB3:
		emu->smm_hdr = (emu->smm_hdr & ~0xFF000000) | ((uint32_t)value << 16);
		break;
	case 0xB8:
		emu->gcr = value;
		break;
	case 0xB9:
		emu->vgactl = value;
		break;
	case 0xBA:
		emu->vgam = (emu->vgam & ~0xFF) | value;
		break;
	case 0xBB:
		emu->vgam = (emu->vgam & ~0xFF00) | (value << 8);
		break;
	case 0xBC:
		emu->vgam = (emu->vgam & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xBD:
		emu->vgam = (emu->vgam & ~0xFF000000) | ((uint32_t)value << 16);
		break;
	case 0xC0:
		emu->ccr[0] = value;
		break;
	case 0xC1:
		emu->ccr[1] = value;
		break;
	case 0xC2:
		emu->ccr[2] = value;
		break;
	case 0xC3:
		emu->ccr[3] = value;
		break;
	case 0xC4:
		emu->arr[0] = (emu->arr[0] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xC5:
		emu->arr[0] = (emu->arr[0] & ~0xFF00) | (value << 8);
		break;
	case 0xC6:
		emu->arr[0] = (emu->arr[0] & ~0xFF) | value;
		break;
	case 0xC7:
		emu->arr[1] = (emu->arr[1] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xC8:
		emu->arr[1] = (emu->arr[1] & ~0xFF00) | (value << 8);
		break;
	case 0xC9:
		emu->arr[1] = (emu->arr[1] & ~0xFF) | value;
		break;
	case 0xCA:
		emu->arr[2] = (emu->arr[2] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xCB:
		emu->arr[2] = (emu->arr[2] & ~0xFF00) | (value << 8);
		break;
	case 0xCC:
		emu->arr[2] = (emu->arr[2] & ~0xFF) | value;
		break;
	case 0xCD:
		emu->arr[3] = (emu->arr[3] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xCE:
		emu->arr[3] = (emu->arr[3] & ~0xFF00) | (value << 8);
		break;
	case 0xCF:
		emu->arr[3] = (emu->arr[3] & ~0xFF) | value;
		break;
	case 0xD0:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[12] = (emu->arr[12] & ~0xFF0000) | ((uint32_t)value << 16);
		else
			emu->arr[4] = (emu->arr[4] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xD1:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[12] = (emu->arr[12] & ~0xFF00) | (value << 8);
		else
			emu->arr[4] = (emu->arr[4] & ~0xFF00) | (value << 8);
		break;
	case 0xD2:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[12] = (emu->arr[12] & ~0xFF) | value;
		else
			emu->arr[4] = (emu->arr[4] & ~0xFF) | value;
		break;
	case 0xD3:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[13] = (emu->arr[13] & ~0xFF0000) | ((uint32_t)value << 16);
		else
			emu->arr[5] = (emu->arr[5] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xD4:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[13] = (emu->arr[13] & ~0xFF00) | (value << 8);
		else
			emu->arr[5] = (emu->arr[5] & ~0xFF00) | (value << 8);
		break;
	case 0xD5:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->arr[13] = (emu->arr[13] & ~0xFF) | value;
		else
			emu->arr[5] = (emu->arr[5] & ~0xFF) | value;
		break;
	case 0xD6:
		emu->arr[6] = (emu->arr[6] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xD7:
		emu->arr[6] = (emu->arr[6] & ~0xFF00) | (value << 8);
		break;
	case 0xD8:
		emu->arr[6] = (emu->arr[6] & ~0xFF) | value;
		break;
	case 0xD9:
		emu->arr[7] = (emu->arr[7] & ~0xFF0000) | ((uint32_t)value << 16);
		break;
	case 0xDA:
		emu->arr[7] = (emu->arr[7] & ~0xFF00) | (value << 8);
		break;
	case 0xDB:
		emu->arr[7] = (emu->arr[7] & ~0xFF) | value;
		break;
	case 0xDC:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[8] = value;
		else
			emu->rcr[0] = value;
		break;
	case 0xDD:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[9] = value;
		else
			emu->rcr[1] = value;
		break;
	case 0xDE:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[10] = value;
		else
			emu->rcr[2] = value;
		break;
	case 0xDF:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[11] = value;
		else
			emu->rcr[3] = value;
		break;
	case 0xE0:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[12] = value;
		else
			emu->rcr[4] = value;
		break;
	case 0xE1:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[13] = value;
		else
			emu->rcr[5] = value;
		break;
	case 0xE2:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[14] = value;
		else
			emu->rcr[6] = value;
		break;
	case 0xE3:
		if(emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_III && (emu->ccr[3] & 0x20) != 0)
			emu->rcr[15] = value;
		else
			emu->rcr[7] = value;
		break;
	case 0xE8:
		emu->ccr[4] = value;
		break;
	case 0xE9:
		emu->ccr[5] = value;
		break;
	case 0xEA:
		emu->ccr[6] = value;
		break;
	case 0xEB:
		emu->ccr[7] = value;
		break;
	case 0xF0:
		emu->pcr[1] = value;
		break;
	case 0xFB:
		emu->dir[4] = value;
		break;
	case 0xFC:
		emu->dir[3] = value;
		break;
	case 0xFD:
		emu->dir[2] = value;
		break;
	case 0xFE:
		emu->dir[0] = value;
		break;
	case 0xFF:
		emu->dir[1] = value;
		break;
	}
}

// 8080/Z80 registers

// Synchronizes 8080/Z80 registers to x86 values
static inline void x86_load_x80_registers(x86_state_t * emu)
{
	emu->x80.bank[emu->x80.af_bank].af = (x86_register_get8_low(emu, X86_R_AX) << 8) | x86_flags_get8(emu);
	emu->x80.bank[emu->x80.main_bank].bc = x86_register_get16(emu, X86_R_CX);
	emu->x80.bank[emu->x80.main_bank].de = x86_register_get16(emu, X86_R_DX);
	emu->x80.bank[emu->x80.main_bank].hl = x86_register_get16(emu, X86_R_BX);
	emu->x80.pc = emu->xip;
	emu->x80.sp = x86_register_get16(emu, X86_R_BP);
	if(x86_is_z80(emu))
	{
		emu->x80.ix = x86_register_get16(emu, X86_R_SI);
		emu->x80.iy = x86_register_get16(emu, X86_R_DI);
		emu->x80.iff1 = emu->_if != 0 ? 1 : 0;
	}
}

// Synchronizes x86 registers to 8080/Z80 values
static inline void x86_store_x80_registers(x86_state_t * emu)
{
	if(x86_is_emulation_mode(emu))
	{
		x86_register_set8_low(emu, X86_R_AX, emu->x80.bank[emu->x80.af_bank].af >> 8);
		x86_flags_set8(emu, emu->x80.bank[emu->x80.af_bank].af);
		x86_register_set16(emu, X86_R_CX, emu->x80.bank[emu->x80.main_bank].bc);
		x86_register_set16(emu, X86_R_DX, emu->x80.bank[emu->x80.main_bank].de);
		x86_register_set16(emu, X86_R_BX, emu->x80.bank[emu->x80.main_bank].hl);
		x86_set_xip(emu, emu->x80.pc);
		x86_register_set16(emu, X86_R_BP, emu->x80.sp);
		if(x86_is_z80(emu))
		{
			x86_register_set16(emu, X86_R_SI, emu->x80.ix);
			x86_register_set16(emu, X86_R_DI, emu->x80.iy);
			emu->_if = emu->x80.iff1 != 0 ? X86_FL_IF : 0;
		}
	}
}

// NEC V25/V55 special function registers

static inline uint8_t x86_sfr_get(x86_state_t * emu, uint16_t index)
{
	switch(emu->cpu_type)
	{
	case X86_CPU_V25:
		return emu->iram[index];
	case X86_CPU_V55:
		return x86_memory_read8(emu, 0xFFE00 + index);
	default:
		assert(false);
	}
}

static inline uint16_t x86_sfr_get16(x86_state_t * emu, uint16_t index)
{
	return x86_sfr_get(emu, index) | (x86_sfr_get(emu, index + 1) << 8);
}

static inline void x86_sfr_set(x86_state_t * emu, uint16_t index, uint8_t value)
{
	switch(emu->cpu_type)
	{
	case X86_CPU_V25:
		emu->iram[index] = value;
		break;
	case X86_CPU_V55:
		x86_memory_write8(emu, 0xFFE00 + index, value);
		break;
	default:
		assert(false);
	}
}

static inline void x86_sfr_set16(x86_state_t * emu, uint16_t index, uint8_t value)
{
	x86_sfr_set(emu, index,     value);
	x86_sfr_set(emu, index + 1, value >> 8);
}


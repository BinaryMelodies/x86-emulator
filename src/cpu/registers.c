
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

static inline uint32_t x86_test_register_get32(x86_state_t * emu, int number)
{
	if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

	// TODO: which flags are allowed
	switch(number)
	{
	case 3:
	case 4:
	case 5:
		if(emu->cpu_type < X86_CPU_486)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		break;
	case 6:
	case 7:
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	return emu->tr386[number];
}

// TODO: according to Wikipedia, 386 has undocumented TR4/TR5, Cyrix 5x86+ has TR1/TR2, Geode GX2 and AMD (not NatSemi) GX has all 8, WinChip behaves as NOP
static inline void x86_test_register_set32(x86_state_t * emu, int number, uint32_t value)
{
	if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

	// TODO: what happens on invalid flags
	switch(number)
	{
	case 3:
	case 5:
		if(emu->cpu_type < X86_CPU_486)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		break;
	case 4:
		if(emu->cpu_type < X86_CPU_486)
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
		else
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
	default:
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	emu->tr386[number] = value;
}

// Model specific registers

static inline bool x86_msr_is_valid(x86_state_t * emu, uint32_t index)
{
	switch(index)
	{
	// Intel
	case X86_R_TSC:
		return (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_TSC) != 0;
	case X86_R_SYSENTER_CS:
		return (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_SEP) != 0;
	case X86_R_SYSENTER_ESP:
	case X86_R_SYSENTER_EIP:
		return (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_SEP) != 0;
	case X86_R_BNDCFGS:
		return (emu->cpu_traits.cpuid7_0.ebx & X86_CPUID7_0_EBX_MPX) != 0;

	// Cyrix
	case X86_R_GX2_PCR:
	case X86_R_SMM_CTL:
	case X86_R_DMI_CTL:
	case X86_R_SMM_HDR:
	case X86_R_DMM_HDR:
	case X86_R_SMM_BASE:
	case X86_R_DMM_BASE:
		return emu->cpu_type == X86_CPU_CYRIX && emu->cpu_traits.cpu_subtype == X86_CPU_CYRIX_LX;

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
	case X86_R_LSTAR:
	case X86_R_CSTAR:
	case X86_R_FMASK:
		if(emu->cpu_type == X86_CPU_AMD)
		{
			return (emu->cpu_traits.cpuid_ext1.edx & (X86_CPUID_EXT1_EDX_SYSCALL_K6 | X86_CPUID_EXT1_EDX_SYSCALL)) != 0;
		}
		else
		{
			return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
		}
	case X86_R_FS_BAS:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	case X86_R_GS_BAS:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
	case X86_R_KERNEL_GS_BAS:
		return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
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
	case X86_R_TSC:
		return emu->tsc;
	case X86_R_SYSENTER_CS:
		return emu->sysenter_cs;
	case X86_R_SYSENTER_ESP:
		return emu->sysenter_esp;
	case X86_R_SYSENTER_EIP:
		return emu->sysenter_eip;
	case X86_R_BNDCFGS:
		return emu->bndcfgs;

	case X86_R_GX2_PCR:
		return emu->gx2_pcr;
	case X86_R_SMM_CTL:
		return emu->smm_ctl;
	case X86_R_DMI_CTL:
		return emu->dmi_ctl;
	case X86_R_SMM_HDR:
		return emu->smm_hdr;
	case X86_R_DMM_HDR:
		return emu->dmm_hdr;
	case X86_R_SMM_BASE:
		return emu->smm.base | ((uint64_t)emu->smm.limit << 32);
	case X86_R_DMM_BASE:
		return emu->dmm.base | ((uint64_t)emu->dmm.limit << 32);

	case X86_R_EFER:
		return emu->efer;
	case X86_R_STAR:
		return emu->star;
	case X86_R_LSTAR:
		return emu->lstar;
	case X86_R_CSTAR:
		return emu->cstar;
	case X86_R_FMASK:
		return emu->fmask;
	case X86_R_FS_BAS:
		return emu->sr[X86_R_FS].base;
	case X86_R_GS_BAS:
		return emu->sr[X86_R_GS].base;
	case X86_R_KERNEL_GS_BAS:
		return emu->kernel_gs_bas;
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
	case X86_R_TSC:
		emu->tsc = value;
		break;
	case X86_R_SYSENTER_CS:
		emu->sysenter_cs = value;
		break;
	case X86_R_SYSENTER_ESP:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sysenter_esp = value;
		break;
	case X86_R_SYSENTER_EIP:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sysenter_eip = value;
		break;
	case X86_R_BNDCFGS:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->bndcfgs = value;
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
	case X86_R_SMM_HDR:
		emu->smm_hdr = value;
		break;
	case X86_R_DMM_HDR:
		emu->dmm_hdr = value;
		break;
	case X86_R_SMM_BASE:
		emu->smm.base = value;
		emu->smm.limit = value >> 32;
		break;
	case X86_R_DMM_BASE:
		emu->dmm.base = value;
		emu->dmm.limit = value >> 32;
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
	case X86_R_FMASK:
		emu->fmask = value;
		break;
	case X86_R_FS_BAS:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sr[X86_R_FS].base = value;
		break;
	case X86_R_GS_BAS:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->sr[X86_R_GS].base = value;
		break;
	case X86_R_KERNEL_GS_BAS:
		x86_check_canonical_address(emu, NONE, value, 0);
		emu->kernel_gs_bas = value;
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


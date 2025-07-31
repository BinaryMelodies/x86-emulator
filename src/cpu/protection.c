
//// Handling protected mode structures, such as privilege levels, descriptors and segments

// Current privilege level, stored in various places for various CPUs, we store it as a separate structure entry

static inline unsigned x86_get_cpl(x86_state_t * emu)
{
	return emu->cpl;
}

static inline void x86_set_cpl(x86_state_t * emu, unsigned rpl)
{
	rpl &= X86_SEL_RPL_MASK;
	emu->cpl = rpl;
	/* 386 stores it in SS, here we store it in both SS and CS */
	emu->sr[X86_R_SS].access = (emu->sr[X86_R_SS].access & ~(3 << X86_DESC_DPL_SHIFT)) | (rpl << X86_DESC_DPL_SHIFT);
	emu->sr[X86_R_CS].access = (emu->sr[X86_R_CS].access & ~(3 << X86_DESC_DPL_SHIFT)) | (rpl << X86_DESC_DPL_SHIFT);
	if(!x86_is_real_mode(emu) && !x86_is_virtual_8086_mode(emu))
		emu->sr[X86_R_CS].selector = (emu->sr[X86_R_CS].selector & ~X86_SEL_RPL_MASK) | rpl;
}

static inline bool x86_selector_is_null(uint16_t selector)
{
	return (selector & ~X86_SEL_RPL_MASK) == 0;
}

// Interpreting the access word

static inline x86_descriptor_type_t x86_access_get_type(uint16_t access)
{
	return access & X86_DESC_TYPE_MASK;
}

static inline uint16_t x86_access_set_type(uint16_t access, x86_descriptor_type_t type)
{
	return (access & ~X86_DESC_TYPE_MASK) | (type & X86_DESC_TYPE_MASK);
}

static inline bool x86_access_is_executable(uint16_t access)
{
	return (access & X86_DESC_X) != 0;
}

static inline bool x86_access_is_readable(uint16_t access)
{
	// only for code descriptors
	return (access & X86_DESC_R) != 0;
}

static inline bool x86_access_is_writable(uint16_t access)
{
	// only for data descriptors
	return (access & X86_DESC_W) != 0;
}

static inline bool x86_access_is_conforming(uint16_t access)
{
	// only for code descriptors
	return (access & X86_DESC_C) != 0;
}

static inline bool x86_access_is_expand_down(uint16_t access)
{
	// only for data descriptors
	return (access & X86_DESC_E) != 0;
}

static inline unsigned x86_access_get_dpl(uint16_t access)
{
	return (access >> X86_DESC_DPL_SHIFT) & 3;
}

static inline bool x86_descriptor_flags_is_big(uint16_t flags)
{
	return (flags & (X86_DESC_D >> 16)) != 0;
}

static inline bool x86_descriptor_flags_is_long(uint16_t flags)
{
	return (flags & (X86_DESC_L >> 16)) != 0;
}

static inline bool x86_descriptor_flags_is_size_in_pages(uint16_t flags)
{
	return (flags & (X86_DESC_G >> 16)) != 0;
}

// Interpreting descriptor data structures

static inline uint16_t x86_descriptor_get_word(uint8_t * descriptor, size_t offset)
{
	return le16toh(*(uint16_t *)&descriptor[offset << 1]);
}

static inline void x86_descriptor_set_word(uint8_t * descriptor, size_t offset, uint16_t value)
{
	*(uint16_t *)&descriptor[offset << 1] = htole16(value);
}

static inline x86_descriptor_type_t x86_descriptor_get_type(uint8_t * descriptor)
{
	return x86_access_get_type(x86_descriptor_get_word(descriptor, X86_DESCWORD_TYPE));
}

static inline void x86_descriptor_set_type(uint8_t * descriptor, x86_descriptor_type_t type)
{
	x86_descriptor_set_word(descriptor, X86_DESCWORD_TYPE, x86_access_set_type(x86_descriptor_get_word(descriptor, X86_DESCWORD_TYPE), type));
}

static inline bool x86_descriptor_is_present(uint8_t * descriptor)
{
	return (x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & X86_DESC_P) != 0;
}

static inline bool x86_descriptor_is_system_segment(uint8_t * descriptor)
{
	return (x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & X86_DESC_S) == 0;
}

static inline bool x86_descriptor_is_executable(uint8_t * descriptor)
{
	return x86_access_is_executable(x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS));
}

static inline bool x86_descriptor_is_writable(uint8_t * descriptor)
{
	return x86_access_is_writable(x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS));
}

static inline bool x86_descriptor_is_conforming(uint8_t * descriptor)
{
	// only for code segments
	return x86_access_is_conforming(x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS));
}

static inline bool x86_descriptor_is_expand_down(uint8_t * descriptor)
{
	// only for code segments
	return x86_access_is_expand_down(x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS));
}

static inline unsigned x86_descriptor_get_dpl(uint8_t * descriptor)
{
	return x86_access_get_dpl(x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS));
}

static inline bool x86_descriptor_get_parameter_count(uint8_t * descriptor)
{
	// only for 16/32-bit call gate
	return x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & 0x1F;
}

static inline bool x86_descriptor_get_ist(uint8_t * descriptor)
{
	// only for 64-bit interrupt/trap gate
	return x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & 3;
}

static inline bool x86_descriptor_is_big(uint8_t * descriptor)
{
	return x86_descriptor_flags_is_big(x86_descriptor_get_word(descriptor, X86_DESCWORD_FLAGS));
}

static inline bool x86_descriptor_is_long(uint8_t * descriptor)
{
	return x86_descriptor_flags_is_long(x86_descriptor_get_word(descriptor, X86_DESCWORD_FLAGS));
}

static inline bool x86_descriptor_is_size_valid(x86_state_t * emu, uint8_t * descriptor)
{
	if(x86_is_long_mode(emu))
		return !(x86_descriptor_is_big(descriptor) && x86_descriptor_is_long(descriptor));
	else
		return true; // assuming other CPUs/modes ignore other fields
}

static inline x86_descriptor_type_t x86_segment_get_type(x86_segment_t * seg)
{
	return x86_access_get_type(seg->access);
}

static inline bool x86_segment_is_executable(x86_segment_t * seg)
{
	return x86_access_is_executable(seg->access);
}

static inline bool x86_segment_is_readable(x86_segment_t * seg)
{
	// only for code segments
	return x86_access_is_readable(seg->access);
}

static inline bool x86_segment_is_writable(x86_segment_t * seg)
{
	// only for data segments
	return x86_access_is_writable(seg->access);
}

static inline bool x86_segment_is_expand_down(x86_segment_t * seg)
{
	// only for code segments
	return x86_access_is_expand_down(seg->access);
}

static inline uoff_t x86_descriptor_get_limit(x86_state_t * emu, uint8_t * descriptor)
{
	uoff_t limit = x86_descriptor_get_word(descriptor, X86_DESCWORD_SEGMENT_LIMIT0);
	if(emu->cpu_type >= X86_CPU_386)
	{
		limit |= (x86_descriptor_get_word(descriptor, X86_DESCWORD_SEGMENT_LIMIT1) & 0x000F) << 16;
		if(x86_descriptor_flags_is_size_in_pages(x86_descriptor_get_word(descriptor, X86_DESCWORD_FLAGS)))
		{
			limit <<= 12;
			limit |= 0xFFF;
		}
	}
	return limit;
}

// get 32-bit gate offset, for 386+
static inline uint32_t x86_descriptor_get_gate_offset_32(uint8_t * descriptor)
{
	return x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE_OFFSET0)
		| ((uint32_t)x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE_OFFSET1) << 16);
}

// get 64-bit gate offset, in long mode
static inline uint64_t x86_descriptor_get_gate_offset_64(uint8_t * descriptor)
{
	return x86_descriptor_get_gate_offset_32(descriptor)
		| ((uint64_t)x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE64_OFFSET2) << 32)
		| ((uint64_t)x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE64_OFFSET3) << 48);
}

// get gate offset, depending on whether this is a 286 or 386
static inline uint32_t x86_descriptor_get_gate_offset_386(x86_state_t * emu, uint8_t * descriptor)
{
	uint32_t offset = x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE_OFFSET0);
	if(emu->cpu_type >= X86_CPU_386)
		offset |= (uint32_t)x86_descriptor_get_word(descriptor, X86_DESCWORD_GATE_OFFSET1) << 16;
	return offset;
}

// get gate offset
static inline uaddr_t x86_descriptor_get_gate_offset(x86_state_t * emu, uint8_t * descriptor)
{
	return x86_is_long_mode(emu)
		? x86_descriptor_get_gate_offset_64(descriptor)
		: x86_descriptor_get_gate_offset_386(emu, descriptor);
}

//// Limit checks

// Checks limits before loading segment in 16/32-bit mode
static inline void x86_descriptor_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uint8_t * descriptor, uoff_t offset, uoff_t size, uoff_t error_code)
{
	uoff_t limit = x86_descriptor_get_limit(emu, descriptor);

	if(x86_descriptor_is_executable(descriptor) || !x86_descriptor_is_expand_down(descriptor))
	{
		if(x86_overflow(offset, size, limit))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			else
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		}
	}
	else if(emu->cpu_type >= X86_CPU_386 && x86_descriptor_is_big(descriptor))
	{
		if(offset <= limit || x86_overflow(offset, size, 0xFFFF))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			else
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		}
	}
	else
	{
		if(offset <= limit || x86_overflow(offset, size, 0xFFFFFFFF))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			else
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		}
	}
}

// Checks limits before loading segment in 64-bit
static inline void x86_descriptor_check_limit_64bit_mode(x86_state_t * emu, x86_segnum_t segment_number, uint8_t * descriptor, uoff_t offset, uoff_t size, uoff_t error_code)
{
	// TODO: LMSLE support is not in CPUID but AMD family/model
	if((emu->efer & X86_EFER_LMSLE) && segment_number != X86_R_CS && segment_number != X86_R_GS)
	{
		uoff_t limit = x86_descriptor_get_limit(emu, descriptor);
		if(x86_overflow(offset, size, 0xFFFFFFFF00000000 + limit))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			else
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		}
	}
}

static inline void x86_segment_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uoff_t size, uoff_t error_code)
{
	if(x86_is_64bit_mode(emu))
	{
		// TODO: LMSLE support is not in CPUID but AMD family/model
		if((emu->efer & X86_EFER_LMSLE) && segment_number != X86_R_CS && segment_number != X86_R_GS)
		{
			if(x86_overflow(offset, size, 0xFFFFFFFF00000000 + emu->sr[segment_number].limit))
			{
				if(segment_number == X86_R_SS)
					x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
				else
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
		}
	}
	else if(emu->cpu_type >= X86_CPU_286)
	{
		if(x86_segment_is_executable(&emu->sr[segment_number]) || !x86_segment_is_expand_down(&emu->sr[segment_number]))
		{
			if(x86_overflow(offset, size, emu->sr[segment_number].limit))
			{
				if(segment_number == X86_R_SS)
					x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
				else
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
		}
		else if(emu->cpu_type >= X86_CPU_386 && x86_segment_is_big(&emu->sr[segment_number]))
		{
			if(offset <= emu->sr[segment_number].limit || x86_overflow(offset, size, 0xFFFF))
			{
				if(segment_number == X86_R_SS)
					x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
				else
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
		}
		else
		{
			if(offset <= emu->sr[segment_number].limit || x86_overflow(offset, size, 0xFFFFFFFF))
			{
				if(segment_number == X86_R_SS)
					x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
				else
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
		}
	}
}

// 80287 and 80387 invoke a separate interrupt when accessing beyond the first 2 bytes of the memory operand fails
static inline void x87_segment_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uoff_t size, uoff_t error_code)
{
	if(X87_FPU_287 <= emu->x87.fpu_type && emu->x87.fpu_type <= X87_FPU_387)
	{
		x86_segment_check_limit(emu, segment_number, x86_offset, 2, error_code);
		if(size >= x86_offset - offset + 2)
		{
			if(x86_segment_is_executable(&emu->sr[segment_number]) || !x86_segment_is_expand_down(&emu->sr[segment_number]))
			{
				if(x86_overflow(offset, size, emu->sr[segment_number].limit))
					x86_trigger_interrupt(emu, X86_EXC_MP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
			else if(emu->cpu_type >= X86_CPU_386 && x86_segment_is_big(&emu->sr[segment_number]))
			{
				if(offset <= emu->sr[segment_number].limit || x86_overflow(offset, size, 0xFFFF))
					x86_trigger_interrupt(emu, X86_EXC_MP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
			else
			{
				if(offset <= emu->sr[segment_number].limit || x86_overflow(offset, size, 0xFFFFFFFF))
					x86_trigger_interrupt(emu, X86_EXC_MP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			}
		}
	}
	else if(emu->x87.fpu_type >= X87_FPU_387)
	{
		x86_segment_check_limit(emu, segment_number, offset, size, error_code);
	}
}

// Check segment limit for stack storage before loading into SS
static inline void x86_stack_descriptor_check_limit(x86_state_t * emu, uint16_t ss, uint8_t * descriptor, uoff_t sp, uoff_t count)
{
	if(x86_descriptor_is_big(descriptor))
		x86_descriptor_check_limit(emu, X86_R_SS, descriptor, (sp - count) & 0xFFFF, count, ss);
	else
		x86_descriptor_check_limit(emu, X86_R_SS, descriptor, (sp - count) & 0xFFFFFFFF, count, ss);
}

static inline void x86_stack_descriptor_check_limit_64bit_mode(x86_state_t * emu, uint16_t ss, uint8_t * descriptor, uoff_t sp, uoff_t count)
{
	x86_descriptor_check_limit_64bit_mode(emu, X86_R_SS, descriptor, sp - count, count, ss);
}

static inline void x86_stack_segment_check_limit(x86_state_t * emu, uoff_t count, uoff_t error_code)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			x86_segment_check_limit(emu, X86_R_SS, (sp - count) & 0xFFFF, count, error_code);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			x86_segment_check_limit(emu, X86_R_SS, (esp - count) & 0xFFFFFFFF, count, error_code);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			x86_segment_check_limit(emu, X86_R_SS, rsp - count, count, error_code);
		}
		break;
	default:
		assert(false);
	}
}

// Limit checks for GDT/LDT/IDT
static inline void x86_table_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t offset, uoff_t size, int exception, uoff_t error_code)
{
	if(x86_overflow(offset, size, emu->sr[segment_number].limit))
	{
		x86_trigger_interrupt(emu, exception, error_code);
	}
}

// Limit checks for GDT/LDT
static inline void x86_table_check_limit_selector(x86_state_t * emu, uint16_t selector, uaddr_t selector_offset, uoff_t size, x86_exception_t exception_number)
{
	x86_table_check_limit(emu, selector & 4 ? X86_R_LDTR : X86_R_GDTR, (selector & ~7) + selector_offset, size,
		exception_number | X86_EXC_FAULT | X86_EXC_VALUE, selector);
}

// Limit checks for IDT
static inline void x86_table_check_limit_exception(x86_state_t * emu, uint8_t exception_number, uoff_t entry_size, uoff_t error_code)
{
	x86_table_check_limit(emu, X86_R_IDTR, exception_number * entry_size, entry_size,
		X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
}

//// Type checks

// Verifies if segment is readable
static inline void x86_segment_check_read(x86_state_t * emu, x86_segnum_t segment_number)
{
	if(x86_is_protected_mode(emu) && !x86_is_virtual_8086_mode(emu))
	{
		if((emu->sr[segment_number].selector & ~X86_SEL_RPL_MASK) == 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	}
}

// Verifies if segment is writable
static inline void x86_segment_check_write(x86_state_t * emu, x86_segnum_t segment_number)
{
	if(x86_is_protected_mode(emu) && !x86_is_virtual_8086_mode(emu))
	{
		if((emu->sr[segment_number].selector & ~X86_SEL_RPL_MASK) == 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

		if(x86_segment_is_executable(&emu->sr[segment_number]) || !x86_segment_is_writable(&emu->sr[segment_number]))
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	}
}

// Loading descriptors

static inline void x86_descriptor_read_selector(x86_state_t * emu, uint16_t selector, size_t offset, uint8_t * data, size_t size, x86_exception_t exception_number)
{
	x86_table_check_limit_selector(emu, selector, offset, size, exception_number);
	x86_memory_segmented_read(emu, selector & X86_SEL_LDT ? X86_R_LDTR : X86_R_GDTR, (selector & X86_SEL_INDEX_MASK) + offset, size, data);
}

static inline void x86_descriptor_write_selector(x86_state_t * emu, uint16_t selector, size_t offset, uint8_t * data, size_t size)
{
	x86_table_check_limit_selector(emu, selector, offset, size, X86_EXC_GP);
	x86_memory_segmented_write(emu, selector & X86_SEL_LDT ? X86_R_LDTR : X86_R_GDTR, (selector & X86_SEL_INDEX_MASK) + offset, size, data);
}

static inline void x86_descriptor_read_exception(x86_state_t * emu, uint8_t exception_number, uoff_t entry_size, uint8_t * data, uoff_t error_code)
{
	x86_table_check_limit_exception(emu, exception_number, entry_size, error_code);
	x86_memory_segmented_read(emu, X86_R_IDTR, exception_number * entry_size, entry_size, data);
}

static inline void x86_descriptor_load(x86_state_t * emu, uint16_t selector, uint8_t * descriptor, x86_exception_t exception_number)
{
	if(emu->cpu_type >= X86_CPU_386)
	{
		x86_descriptor_read_selector(emu, selector, 0, descriptor, 8, exception_number);
	}
	else
	{
		x86_descriptor_read_selector(emu, selector, 0, descriptor, 6, exception_number);
	}
}

// for LDTR, TR
static inline void x86_descriptor_load_long(x86_state_t * emu, uint16_t selector, uint8_t * descriptor, x86_exception_t exception_number)
{
	if(x86_is_long_mode(emu))
	{
		x86_descriptor_read_selector(emu, selector, 0, descriptor, 16, exception_number);
	}
	else if(emu->cpu_type >= X86_CPU_386)
	{
		x86_descriptor_read_selector(emu, selector, 0, descriptor, 8, exception_number);
	}
	else
	{
		x86_descriptor_read_selector(emu, selector, 0, descriptor, 6, exception_number);
	}
}

// if the first 8 bytes of a descriptor are loaded, loads the following 8 bytes
static inline void x86_descriptor_load_extension(x86_state_t * emu, uint16_t selector, uint8_t * descriptor)
{
	x86_descriptor_read_selector(emu, selector, 8, descriptor + 8, 8, X86_EXC_GP);
}

//// Segment registers

static inline void x86_segment_load_protected_mode_286(x86_state_t * emu, x86_segnum_t segment_number, uint16_t selector, uint8_t * descriptor)
{
	if((descriptor[X86_DESCBYTE_ACCESS] & (X86_DESC_A >> 8)) != 0)
	{
		descriptor[X86_DESCBYTE_ACCESS] |= X86_DESC_A >> 8;
		x86_descriptor_write_selector(emu, segment_number, X86_DESCBYTE_ACCESS, &descriptor[X86_DESCBYTE_ACCESS], 1);
	}

	emu->sr[segment_number].selector = selector;
	emu->sr[segment_number].limit = x86_descriptor_get_word(descriptor, 0);
	emu->sr[segment_number].base = x86_descriptor_get_word(descriptor, 1) | ((uint32_t)(x86_descriptor_get_word(descriptor, 2) & 0x00FF) << 16);
	emu->sr[segment_number].access = x86_descriptor_get_word(descriptor, 2) & 0xFF00;

	if(segment_number == X86_R_CS || segment_number == X86_R_SS)
	{
		// CPL should be restored in all fields
		x86_set_cpl(emu, x86_get_cpl(emu));
	}
}

static inline void x86_segment_load_protected_mode_386(x86_state_t * emu, x86_segnum_t segment_number, uint16_t selector, uint8_t * descriptor)
{
	if((descriptor[X86_DESCBYTE_ACCESS] & (X86_DESC_A >> 8)) != 0)
	{
		descriptor[X86_DESCBYTE_ACCESS] |= X86_DESC_A >> 8;
		x86_descriptor_write_selector(emu, segment_number, X86_DESCBYTE_ACCESS, &descriptor[X86_DESCBYTE_ACCESS], 1);
	}

	emu->sr[segment_number].selector = selector;
	emu->sr[segment_number].limit = x86_descriptor_get_word(descriptor, 0);
	emu->sr[segment_number].limit |= (uint32_t)(x86_descriptor_get_word(descriptor, 3) & 0x000F) << 16;
	emu->sr[segment_number].base = x86_descriptor_get_word(descriptor, 1) | ((uint32_t)(x86_descriptor_get_word(descriptor, 2) & 0x00FF) << 16);
	emu->sr[segment_number].base |= (uint32_t)(x86_descriptor_get_word(descriptor, 3) & 0xFF00) << 16;
	emu->sr[segment_number].access = x86_descriptor_get_word(descriptor, 2) & 0xFF00;
	emu->sr[segment_number].access |= (uint32_t)(x86_descriptor_get_word(descriptor, 3) & 0x00F0) << 16;
	if((emu->sr[segment_number].access & X86_DESC_G) != 0)
	{
		emu->sr[segment_number].limit <<= 12;
		emu->sr[segment_number].limit |= 0xFFF;
	}

	if(segment_number == X86_R_CS || segment_number == X86_R_SS)
	{
		// CPL should be restored in all fields
		x86_set_cpl(emu, x86_get_cpl(emu));
	}
}

static inline void x86_segment_store_protected_mode_386(x86_state_t * emu, x86_segnum_t segment_number, uint8_t * descriptor)
{
	uint32_t limit = emu->sr[segment_number].limit;
	if((emu->sr[segment_number].access & X86_DESC_G) != 0)
	{
		limit >>= 12;
	}
	x86_descriptor_set_word(descriptor, 0, emu->sr[segment_number].limit);
	x86_descriptor_set_word(descriptor, 1, emu->sr[segment_number].base);
	x86_descriptor_set_word(descriptor, 2, ((emu->sr[segment_number].base >> 16) & 0x00FF) | (emu->sr[segment_number].access & 0xFF00));
	x86_descriptor_set_word(descriptor, 3,
		((emu->sr[segment_number].limit >> 16) & 0x000F)
		| ((emu->sr[segment_number].access >> 16) & 0x00F0)
		| ((emu->sr[segment_number].base >> 16) & 0xFF00));
}

// only required for LDTR/TR
static inline void x86_segment_load_protected_mode_64(x86_state_t * emu, x86_segnum_t segment_number, uint16_t selector, uint8_t * descriptor)
{
	/* protected mode does not use banking */
	x86_segment_load_protected_mode_386(emu, segment_number, selector, descriptor);
	emu->sr[segment_number].base |= (uint64_t)x86_descriptor_get_word(descriptor, 4) << 32;
	emu->sr[segment_number].base |= (uint64_t)x86_descriptor_get_word(descriptor, 5) << 48;
	x86_check_canonical_address(emu, NONE, emu->sr[segment_number].base, 0); // TODO: check before loading the descriptor
}

static inline void x86_segment_load_null(x86_state_t * emu, x86_segnum_t segment_number, int rpl)
{
	emu->sr[segment_number].selector = rpl & X86_SEL_RPL_MASK;
//	emu->sr[segment_number].access &= ~X86_DESC_P; // TODO: is clearing the descriptor valid bit necessary?
	/* TODO: what else needs to be done? */
}

static inline void x86_segment_load_protected_mode(x86_state_t * emu, x86_segnum_t segment_number, uint16_t selector, uint8_t * descriptor)
{
	if(x86_selector_is_null(selector))
	{
		x86_segment_load_null(emu, segment_number, selector);
	}
	else if(emu->cpu_type < X86_CPU_386)
	{
		x86_segment_load_protected_mode_286(emu, segment_number, selector, descriptor);
	}
	else
	{
		x86_segment_load_protected_mode_386(emu, segment_number, selector, descriptor);
	}
}

static inline void x86_ldtr_load(x86_state_t * emu, uint16_t selector)
{
	uint8_t descriptor[16];
	if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu))
	{
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	else if(x86_selector_is_null(selector))
	{
		if(x86_overflow(0, 0, emu->sr[X86_R_GDTR].limit))
		{
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
		}
		x86_segment_load_null(emu, X86_R_LDTR, selector);
	}
	else
	{
		if((selector & X86_SEL_LDT) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		x86_descriptor_load_long(emu, selector, descriptor, X86_EXC_GP);

		if(x86_descriptor_get_type(descriptor) != X86_DESC_TYPE_LDT)
		{
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
		}

		if(!x86_descriptor_is_present(descriptor))
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		if(x86_is_long_mode(emu))
		{
			x86_segment_load_protected_mode_64(emu, X86_R_LDTR, selector, descriptor);
		}
		else if(emu->cpu_type >= X86_CPU_386)
		{
			x86_segment_load_protected_mode_386(emu, X86_R_LDTR, selector, descriptor);
		}
		else
		{
			x86_segment_load_protected_mode_286(emu, X86_R_LDTR, selector, descriptor);
		}
	}
}

static inline void x86_ldtr_load_switch_task(x86_state_t * emu, uint16_t selector)
{
	// identical to x86_ldtr_load_extended, except issues TS instead of GP
	uint8_t descriptor[8];
	if(x86_selector_is_null(selector))
	{
		if(x86_overflow(0, 0, emu->sr[X86_R_GDTR].limit))
		{
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, selector);
		}
		x86_segment_load_null(emu, X86_R_LDTR, selector);
	}
	else
	{
		if((selector & X86_SEL_LDT) != 0)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		// technically, this could be x86_descriptor_load_long since we load LDTR, but it will never be executed in long mode
		x86_descriptor_load(emu, selector, descriptor, X86_EXC_TS);

		if(x86_descriptor_get_type(descriptor) != X86_DESC_TYPE_LDT)
		{
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, selector);
		}

		if(!x86_descriptor_is_present(descriptor))
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		if(emu->cpu_type >= X86_CPU_386)
		{
			x86_segment_load_protected_mode_386(emu, X86_R_LDTR, selector, descriptor);
		}
		else
		{
			x86_segment_load_protected_mode_286(emu, X86_R_LDTR, selector, descriptor);
		}
	}
}

//// Instructions

// LLTR
static inline void x86_tr_load(x86_state_t * emu, uint16_t selector)
{
	uint8_t descriptor[16];
	if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu))
	{
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	else if(x86_selector_is_null(selector))
	{
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	}
	else
	{
		if((selector & X86_SEL_LDT) != 0)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		x86_descriptor_load_long(emu, selector, descriptor, X86_EXC_GP);

		switch(x86_descriptor_get_type(descriptor))
		{
		case X86_DESC_TYPE_TSS16_A:
			if(x86_is_long_mode(emu) || x86_is_32bit_only(emu))
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
			break;
		case X86_DESC_TYPE_TSS32_A:
			break;

		default:
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
			break;
		}

		if(!x86_descriptor_is_present(descriptor))
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

		descriptor[X86_DESCBYTE_ACCESS] |= X86_DESC_BUSY >> 8;
		x86_descriptor_write_selector(emu, selector, X86_DESCBYTE_ACCESS, &descriptor[X86_DESCBYTE_ACCESS], 1);

		if(x86_is_long_mode(emu))
		{
			x86_segment_load_protected_mode_64(emu, X86_R_TR, selector, descriptor);
		}
		else if(emu->cpu_type >= X86_CPU_386)
		{
			x86_segment_load_protected_mode_386(emu, X86_R_TR, selector, descriptor);
		}
		else
		{
			x86_segment_load_protected_mode_286(emu, X86_R_TR, selector, descriptor);
		}
	}
}

// Note: mirrors x86_segment_get, defined in registers.c
// used by MOV Sw, Ev; POP and LxS
static inline void x86_segment_set(x86_state_t * emu, x86_segnum_t segment_number, uint16_t value)
{
	segment_number = x86_segment_get_number(emu, segment_number);

	if(emu->cpu_type >= X86_CPU_286 && segment_number == X86_R_CS)
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);

	if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu))
	{
		x86_segment_load_real_mode(emu, segment_number, value);
	}
	else
	{
		if(x86_selector_is_null(value))
		{
			if(segment_number == X86_R_SS)
			{
				if(!x86_is_64bit_mode(emu) || x86_get_cpl(emu) == 3 || x86_get_cpl(emu) != value)
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
				if(x86_get_cpl(emu) != (value & X86_SEL_RPL_MASK))
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}

			x86_segment_load_null(emu, segment_number, value);
			return;
		}

		uint8_t descriptor[8];
		x86_descriptor_load(emu, value, descriptor, X86_EXC_GP);

		// TODO: assert D=1?

		if(segment_number == X86_R_SS)
		{
			if(x86_get_cpl(emu) != x86_descriptor_get_dpl(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, value);
			if(x86_descriptor_is_system_segment(descriptor)
			|| x86_descriptor_is_executable(descriptor)
			|| !x86_descriptor_is_writable(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, value);
		}
		else
		{
			if(x86_descriptor_is_system_segment(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, value);

			if(x86_descriptor_is_executable(descriptor) || !x86_descriptor_is_conforming(descriptor))
			{
				unsigned dpl = x86_descriptor_get_dpl(descriptor);
				// TODO: is this really an AND in 64-bit mode?
				if(((value & X86_SEL_RPL_MASK) > dpl) || (x86_get_cpl(emu) > dpl))
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}
		}

		if(!x86_descriptor_is_present(descriptor))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			else
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, value);
		}

		if(emu->cpu_type < X86_CPU_386)
		{
			x86_segment_load_protected_mode_286(emu, segment_number, value, descriptor);
		}
		else
		{
			x86_segment_load_protected_mode_386(emu, segment_number, value, descriptor);
		}
	}
	if(x86_is_ia64(emu) && segment_number == X86_R_SS)
		x86_ia64_intercept(emu, 0); // TODO
}

static inline void x86_segment_set_switch_task(x86_state_t * emu, x86_segnum_t segment_number, uint16_t value)
{
	// identical to x86_segment_set, except issues TS instead of GP (except if not present), and only valid in non-long mode
	if(x86_is_virtual_8086_mode(emu))
	{
		x86_segment_load_real_mode_full(emu, segment_number, value);
	}
	else
	{
		if(x86_selector_is_null(value))
		{
			if(segment_number == X86_R_SS)
			{
				if(!x86_is_64bit_mode(emu) || x86_get_cpl(emu) == 3 || x86_get_cpl(emu) != value)
					x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, 0); // TODO: ?
				if(x86_get_cpl(emu) != (value & X86_SEL_RPL_MASK))
					x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}

			x86_segment_load_null(emu, segment_number, value);
			return;
		}

		uint8_t descriptor[8];
		x86_descriptor_load(emu, value, descriptor, X86_EXC_GP);

		// TODO: assert D=1?

		if(segment_number == X86_R_SS)
		{
			if(x86_get_cpl(emu) != x86_descriptor_get_dpl(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			if(x86_descriptor_is_system_segment(descriptor)
			|| x86_descriptor_is_executable(descriptor)
			|| !x86_descriptor_is_writable(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
		}
		else if(segment_number == X86_R_CS)
		{
			// only for task switching

			if(x86_descriptor_is_system_segment(descriptor)
			|| !x86_descriptor_is_executable(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);

			unsigned dpl = x86_descriptor_get_dpl(descriptor);
			if(x86_descriptor_is_conforming(descriptor))
			{
				if((value & X86_SEL_RPL_MASK) > dpl)
					x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}
			else
			{
				if((value & X86_SEL_RPL_MASK) != dpl)
					x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}
		}
		else
		{
			if(x86_descriptor_is_system_segment(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);

			if(x86_descriptor_is_executable(descriptor) || !x86_descriptor_is_conforming(descriptor))
			{
				unsigned dpl = x86_descriptor_get_dpl(descriptor);
				if(((value & X86_SEL_RPL_MASK) > dpl) || (x86_get_cpl(emu) > dpl))
					x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			}
		}

		if(!x86_descriptor_is_present(descriptor))
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, value);
			else
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, value);
		}

		if(emu->cpu_type < X86_CPU_386)
		{
			x86_segment_load_protected_mode_286(emu, segment_number, value, descriptor);
		}
		else
		{
			x86_segment_load_protected_mode_386(emu, segment_number, value, descriptor);
		}

		if(segment_number == X86_R_CS)
		{
			x86_set_cpl(emu, value);
		}
	}
}

// Cyrix specific
static inline int x86_restore_descriptor(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, x86_segnum_t target_segment)
{
	uint16_t selector;
	uint8_t descriptor[8];
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address, 2, &selector);
	selector = le16toh(selector);
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address + 2, 8, descriptor);
	x86_segment_load_protected_mode_386(emu, target_segment, le16toh(selector), descriptor);
	return (x86_descriptor_get_word(descriptor, 2) >> X86_DESC_DPL_SHIFT) & 3;
}

static inline void x86_save_descriptor(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, x86_segnum_t target_segment)
{
	uint16_t selector;
	uint8_t descriptor[8];
	x86_segment_store_protected_mode_386(emu, target_segment, descriptor);
	selector = htole16(emu->sr[target_segment].selector);
	// TODO: check rights
	x86_memory_segmented_write(emu, segment_number, address, 2, &selector);
	// TODO: check rights
	x86_memory_segmented_write(emu, segment_number, address + 2, 8, descriptor);
}

// LAR
static inline uint16_t x86_load_access_rights16(uint8_t * descriptor)
{
	return (x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & 0xFF00);
}

static inline uint32_t x86_load_access_rights32(uint8_t * descriptor)
{
	return (x86_descriptor_get_word(descriptor, X86_DESCWORD_ACCESS) & 0xFF00) | ((uint32_t)(x86_descriptor_get_word(descriptor, X86_DESCWORD_FLAGS) & 0x00F0) << 16);
}

//// Task switching
static inline uoff_t x86_load_task_stack16(x86_state_t * emu, int dpl, uint16_t * ss)
{
	if(x86_overflow(emu->sr[X86_R_TR].base, dpl * 4 + 4, emu->sr[X86_R_TR].limit))
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, emu->sr[X86_R_TR].selector);
	*ss = x86_memory_segmented_read16(emu, X86_R_TR, dpl * 4 + 4);
	return x86_memory_segmented_read16(emu, X86_R_TR, dpl * 4 + 2);
}

static inline uoff_t x86_load_task_stack32(x86_state_t * emu, int dpl, uint16_t * ss)
{
	if(x86_overflow(emu->sr[X86_R_TR].base, dpl * 4 + 8, emu->sr[X86_R_TR].limit))
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, emu->sr[X86_R_TR].selector);
	*ss = x86_memory_segmented_read16(emu, X86_R_TR, dpl * 8 + 8);
	return x86_memory_segmented_read32(emu, X86_R_TR, dpl * 8 + 4);
}

static inline uoff_t x86_load_task_stack64(x86_state_t * emu, int dpl, uint16_t * ss)
{
	if(x86_overflow(emu->sr[X86_R_TR].base, dpl * 8 + 12, emu->sr[X86_R_TR].limit))
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, emu->sr[X86_R_TR].selector);
	*ss = dpl; /* NULL segment */
	return x86_memory_segmented_read64(emu, X86_R_TR, dpl * 8 + 4);
}

static inline uoff_t x86_load_task_ist_stack(x86_state_t * emu, int ist)
{
	if(x86_overflow(emu->sr[X86_R_TR].base, ist * 8 + 0x2C, emu->sr[X86_R_TR].limit))
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, emu->sr[X86_R_TR].selector);
	return x86_memory_segmented_read64(emu, X86_R_TR, ist * 8 + 0x24);
}

static inline uoff_t x86_load_task_stack(x86_state_t * emu, int dpl, uint16_t * ss)
{
	switch(x86_access_get_type(emu->sr[X86_R_TR].access))
	{
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
		return x86_load_task_stack16(emu, dpl, ss);
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		if(x86_is_long_mode(emu))
			return x86_load_task_stack64(emu, dpl, ss);
		else
			return x86_load_task_stack32(emu, dpl, ss);
	default:
		assert(false);
	}
}

static inline int x86_switch_task(x86_state_t * emu, uint16_t tss_selector, uint8_t * tss_descriptor)
{
	int selector_count = 0;

	switch(x86_segment_get_type(&emu->sr[X86_R_TR]))
	{
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
		x86_memory_segmented_write16(emu, X86_R_TR, 0x0E, emu->xip);
		x86_memory_segmented_write16(emu, X86_R_TR, 0x10, x86_flags_get16(emu));
		for(int register_number = 0; register_number < 8; register_number++)
		{
			x86_memory_segmented_write16(emu, X86_R_TR, 0x12 + 2 * register_number, emu->gpr[X86_R_AX + register_number]);
		}
		for(int register_number = 0; register_number < 4; register_number++)
		{
			x86_memory_segmented_write16(emu, X86_R_TR, 0x22 + 2 * register_number, emu->sr[X86_R_ES + register_number].selector);
		}
		break;
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		x86_memory_segmented_write32(emu, X86_R_TR, 0x20, emu->xip);
		x86_memory_segmented_write32(emu, X86_R_TR, 0x24, x86_flags_get32(emu));
		for(int register_number = 0; register_number < 8; register_number++)
		{
			x86_memory_segmented_write32(emu, X86_R_TR, 0x28 + 4 * register_number, emu->gpr[X86_R_AX + register_number]);
		}
		for(int register_number = 0; register_number < 6; register_number++)
		{
			x86_memory_segmented_write16(emu, X86_R_TR, 0x48 + 4 * register_number, emu->sr[X86_R_ES + register_number].selector);
		}
		break;
	default:
		assert(false);
	}

	// TODO: check that all the new TSS and segment descriptors are paged into memory

	// up until this point, the CPU guarantees that it can return to the previous state on an exception

	if(emu->cpu_type >= X86_CPU_386)
	{
		x86_segment_load_protected_mode_386(emu, X86_R_TR, tss_selector, tss_descriptor);
	}
	else
	{
		x86_segment_load_protected_mode_286(emu, X86_R_TR, tss_selector, tss_descriptor);
	}

	uint32_t pdbr = 0;

	switch(x86_segment_get_type(&emu->sr[X86_R_TR]))
	{
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
		x86_set_xip(emu, x86_memory_segmented_read16(emu, X86_R_TR, 0x0E));
		x86_flags_set16(emu, x86_memory_segmented_read16(emu, X86_R_TR, 0x10));
		for(int register_number = 0; register_number < 8; register_number++)
		{
			emu->gpr[X86_R_AX + register_number] = x86_memory_segmented_read16(emu, X86_R_TR, 0x12 + 2 * register_number);
		}
		emu->sr[X86_R_LDTR].selector = x86_memory_segmented_read16(emu, X86_R_TR, 0x2A);
		selector_count = 4;
		for(int register_number = 0; register_number < selector_count; register_number++)
		{
			// only load the selectors
			//x86_segment_set(emu, X86_R_CS + register_number, x86_memory_segmented_read16(emu, X86_R_TR, 0x22 + 2 * register_number));
			emu->sr[X86_R_CS + register_number].selector = x86_memory_segmented_read16(emu, X86_R_TR, 0x22 + 2 * register_number);
		}
		break;
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		x86_set_xip(emu, x86_memory_segmented_read32(emu, X86_R_TR, 0x20));
		x86_flags_set32(emu, x86_memory_segmented_read32(emu, X86_R_TR, 0x24));
		for(int register_number = 0; register_number < 8; register_number++)
		{
			emu->gpr[X86_R_AX + register_number] = x86_memory_segmented_read32(emu, X86_R_TR, 0x28 + 4 * register_number);
		}
		pdbr = x86_memory_segmented_read32(emu, X86_R_TR, 0x1C);
		if((emu->cr[0] == X86_CR0_PG) != 0)
		{
			emu->cr[3] = pdbr;
		}
		emu->sr[X86_R_LDTR].selector = x86_memory_segmented_read16(emu, X86_R_TR, 0x60);
		selector_count = 6;
		for(int register_number = 0; register_number < selector_count; register_number++)
		{
			// only load the selectors
			//x86_segment_set(emu, X86_R_CS + register_number, x86_memory_segmented_read16(emu, X86_R_TR, 0x48 + 2 * register_number));
			emu->sr[X86_R_CS + register_number].selector = x86_memory_segmented_read16(emu, X86_R_TR, 0x22 + 2 * register_number);
		}
		break;
	default:
		assert(false);
	}

	emu->cr[0] |= X86_CR0_TS;

	emu->dr[7] &= ~(X86_DR7_L0 | X86_DR7_L1 | X86_DR7_L2 | X86_DR7_L3);

	// TODO: what is the proper placement of this check?
	if((x86_memory_segmented_read8(emu, X86_R_TR, 0x64) & 0x01) != 0)
	{
		emu->dr[6] |= X86_DR6_BT;
		x86_trigger_interrupt(emu, X86_EXC_DB | X86_EXC_TRAP, 0);
	}

	return selector_count;
}

// Note: this should only be called if the TSS size has already been ascertained
static inline void x86_clear_old_nt(x86_state_t * emu, uint16_t access, uint32_t tss_base)
{
	uint32_t address;

	switch(x86_access_get_type(access))
	{
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
		address = tss_base + 0x10;
		break;
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		address = tss_base + 0x24;
		break;
	default:
		assert(false);
	}

	x86_memory_write8(emu, address,
		x86_memory_read8(emu, address) & ~(X86_FL_NT >> 8));
}

static inline void x86_load_selectors_after_switch(x86_state_t * emu, int selector_count)
{
	x86_ldtr_load_switch_task(emu, emu->sr[X86_R_LDTR].selector);

	for(int register_number = 0; register_number < selector_count; register_number++)
	{
		x86_segment_set_switch_task(emu, X86_R_CS + register_number, emu->sr[register_number].selector);
	}
}

static inline uint8_t x86_tss_busy_byte(x86_state_t * emu)
{
	return (emu->sr[X86_R_TR].access >> 8) & 0xFF;
}

static inline void x86_tss_busy_clear(x86_state_t * emu, uint16_t selector, uint8_t access)
{
	access &= ~(X86_DESC_BUSY >> 8);
	x86_descriptor_write_selector(emu, selector, X86_DESCBYTE_ACCESS, &access, 1);
}

static inline void x86_tss_busy_set(x86_state_t * emu)
{
	uint8_t access = (emu->sr[X86_R_TR].access  >> 8) & 0xFF;
	access |= X86_DESC_BUSY >> 8;
	x86_descriptor_write_selector(emu, emu->sr[X86_R_TR].selector, X86_DESCBYTE_ACCESS, &access, 1);
}

static inline void x86_tss_set_nt(x86_state_t * emu)
{
	emu->nt = X86_FL_NT;
	switch(x86_segment_get_type(&emu->sr[X86_R_TR]))
	{
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
		x86_memory_segmented_write16(emu, X86_R_TR, 0x10,
			x86_memory_segmented_read16(emu, X86_R_TR, 0x10) | X86_FL_NT);
		break;
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		x86_memory_segmented_write16(emu, X86_R_TR, 0x24,
			x86_memory_segmented_read16(emu, X86_R_TR, 0x24) | X86_FL_NT);
		break;
	default:
		assert(false);
	}
}

static inline void x86_tss_set_link(x86_state_t * emu, uint16_t link)
{
	x86_memory_segmented_write16(emu, X86_R_TR, 0x00, link);
}

//// Execution transfers

static inline void x86_jump_via_call_gate(x86_state_t * emu, uint16_t gate_selector, uint8_t * gate_descriptor)
{
	uint16_t segment_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR);
	unsigned cpl = x86_get_cpl(emu);
	unsigned gate_dpl = x86_descriptor_get_dpl(gate_descriptor);
	unsigned segment_rpl = segment_selector & X86_SEL_RPL_MASK;

	if(gate_dpl < cpl || gate_dpl < segment_rpl)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(!x86_descriptor_is_present(gate_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	if(x86_selector_is_null(segment_selector))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);

	uint8_t segment_descriptor[8];
	x86_table_check_limit_selector(emu, segment_selector, 0, 8, X86_EXC_GP);
	x86_descriptor_load(emu, segment_selector, segment_descriptor, X86_EXC_GP);

	if(x86_descriptor_is_system_segment(segment_descriptor) || !x86_descriptor_is_executable(segment_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector);

	unsigned dpl = x86_descriptor_get_dpl(segment_descriptor);
	if(x86_descriptor_is_conforming(segment_descriptor))
	{
		if(dpl > cpl)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector);
	}
	else
	{
		if(dpl != cpl)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector);
	}

	// TODO: assert D=1?

	if(!x86_descriptor_is_size_valid(emu, segment_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector);

	if(!x86_descriptor_is_present(segment_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector);

	uoff_t offset = x86_descriptor_get_gate_offset(emu, gate_descriptor);
	if(!(x86_is_long_mode(emu) && x86_descriptor_is_long(segment_descriptor)))
		x86_descriptor_check_limit(emu, X86_R_CS, segment_descriptor, offset, 1, 0);
	else
		x86_descriptor_check_limit_64bit_mode(emu, X86_R_CS, segment_descriptor, offset, 1, 0);
	x86_check_canonical_address(emu, X86_R_CS, offset, 0);

	x86_segment_load_protected_mode(emu, X86_R_CS, (segment_selector & X86_SEL_INDEX_MASK) | x86_get_cpl(emu), segment_descriptor);
	x86_set_xip(emu, offset);
}

static inline void x86_jump_via_task_gate(x86_state_t * emu, uint16_t gate_selector, uint8_t * gate_descriptor)
{
	uint16_t tss_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR);
	unsigned cpl = x86_get_cpl(emu);
	unsigned dpl = x86_descriptor_get_dpl(gate_descriptor);
	unsigned tss_rpl = tss_selector & X86_SEL_RPL_MASK;

	if(dpl < cpl || dpl < tss_rpl)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(!x86_descriptor_is_present(gate_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	if((tss_selector & X86_SEL_LDT) != 0)
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint8_t tss_descriptor[8];
	x86_table_check_limit_selector(emu, tss_selector, 0, 8, X86_EXC_GP);
	x86_descriptor_load(emu, tss_selector, tss_descriptor, X86_EXC_GP);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint16_t old_tr = emu->sr[X86_R_TR].selector;
	uint8_t busy_byte = x86_tss_busy_byte(emu);
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	x86_tss_busy_clear(emu, old_tr, busy_byte);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	x86_tss_busy_set(emu);

	x86_load_selectors_after_switch(emu, selector_count);
}

static inline void x86_jump_via_task_segment(x86_state_t * emu, uint16_t tss_selector, uint8_t * tss_descriptor)
{
	unsigned cpl = x86_get_cpl(emu);
	unsigned dpl = x86_descriptor_get_dpl(tss_descriptor);
	unsigned rpl = tss_selector & X86_SEL_RPL_MASK;

	if(dpl < cpl || dpl < rpl)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	uint16_t old_tr = emu->sr[X86_R_TR].selector;
	uint8_t busy_byte = x86_tss_busy_byte(emu);
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	x86_tss_busy_clear(emu, old_tr, busy_byte);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	x86_tss_busy_set(emu);
	x86_load_selectors_after_switch(emu, selector_count);
}

static inline void x86_jump_via(x86_state_t * emu, uint16_t selector, uint8_t * descriptor)
{
	switch(x86_descriptor_get_type(descriptor))
	{
	case X86_DESC_TYPE_CALLGATE16:
		x86_jump_via_call_gate(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_CALLGATE32:
		x86_jump_via_call_gate(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_TASKGATE:
		x86_jump_via_task_gate(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		x86_jump_via_task_segment(emu, selector, descriptor);
		break;
	default:
		assert(false);
	}
}

static inline void x86_jump_far(x86_state_t * emu, uint16_t selector, uoff_t offset)
{
	if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu))
	{
		x86_segment_check_limit(emu, X86_R_CS, offset, 1, 0);
		x86_segment_load_real_mode(emu, X86_R_CS, selector);
		x86_set_xip(emu, offset);
	}
	else
	{
		uint8_t descriptor[16];
		x86_table_check_limit_selector(emu, selector, 0, 8, X86_EXC_GP);
		x86_descriptor_load(emu, selector, descriptor, X86_EXC_GP);
		if(!x86_descriptor_is_system_segment(descriptor))
		{
			if(!x86_descriptor_is_executable(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

			if(!x86_descriptor_is_size_valid(emu, descriptor))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

			unsigned cpl = x86_get_cpl(emu);
			unsigned dpl = x86_descriptor_get_dpl(descriptor);
			if(x86_descriptor_is_conforming(descriptor))
			{
				if(dpl > cpl)
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
			}
			else
			{
				unsigned rpl = selector & X86_SEL_RPL_MASK;
				if(rpl > cpl || dpl != cpl)
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
			}

			if(!x86_descriptor_is_present(descriptor))
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector);

			if(!(x86_is_long_mode(emu) && x86_descriptor_is_long(descriptor)))
				x86_descriptor_check_limit(emu, X86_R_CS, descriptor, offset, 1, 0);
			else
				x86_descriptor_check_limit_64bit_mode(emu, X86_R_CS, descriptor, offset, 1, 0);

			x86_segment_load_protected_mode(emu, X86_R_CS, (selector & X86_SEL_INDEX_MASK) | cpl, descriptor);
			x86_set_xip(emu, offset);
		}
		else
		{
			switch(x86_descriptor_get_type(descriptor))
			{
			case X86_DESC_TYPE_CALLGATE32:
				if(emu->cpu_type < X86_CPU_386)
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
				break;
			case X86_DESC_TYPE_TASKGATE:
				if(x86_is_long_mode(emu))
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
				break;
			case X86_DESC_TYPE_CALLGATE16:
			case X86_DESC_TYPE_TSS16_A:
			case X86_DESC_TYPE_TSS16_B:
				if(x86_is_long_mode(emu) || x86_is_32bit_only(emu))
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
				break;
			case X86_DESC_TYPE_TSS32_A:
			case X86_DESC_TYPE_TSS32_B:
				if(emu->cpu_type < X86_CPU_386)
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
				if(x86_is_long_mode(emu))
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
				break;
			default:
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector);
			}

			if(x86_is_long_mode(emu))
			{
				x86_table_check_limit_selector(emu, selector, 8, 8, X86_EXC_GP);
				x86_descriptor_load_extension(emu, selector, descriptor);
			}
			x86_jump_via(emu, selector, descriptor);
		}
	}
}

#define __OPEN(...) __VA_ARGS__

#define __IF_16_16(__body) __OPEN __body
#define __IF_16_32(__body)
#define __IF_16_64(__body)
#define _IF_16(__size, __body) __IF_16_##__size(__body)

#define __IF_NOT16_16(__body)
#define __IF_NOT16_32(__body) __OPEN __body
#define __IF_NOT16_64(__body) __OPEN __body
#define _IF_NOT16(__size, __body) __IF_NOT16_##__size(__body)

#define __IF_64_16(__body)
#define __IF_64_32(__body)
#define __IF_64_64(__body) __OPEN __body
#define _IF_64(__size, __body) __IF_64_##__size(__body)

#define __IF_NOT64_16(__body) __OPEN __body
#define __IF_NOT64_32(__body) __OPEN __body
#define __IF_NOT64_64(__body)
#define _IF_NOT64(__size, __body) __IF_NOT64_##__size(__body)

#define _DEFINE_x86_call_via_call_gate(__size) \
static inline void x86_call_via_call_gate##__size(x86_state_t * emu, uint16_t gate_selector, uint8_t * gate_descriptor) \
{ \
	uint16_t segment_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR); \
	unsigned cpl = x86_get_cpl(emu); \
	unsigned gate_dpl = x86_descriptor_get_dpl(gate_descriptor); \
	unsigned gate_rpl = gate_selector & X86_SEL_RPL_MASK; \
 \
	if(gate_dpl < cpl || gate_rpl > gate_dpl) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector); \
 \
	if(!x86_descriptor_is_present(gate_descriptor)) \
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector); \
 \
	_IF_NOT64(__size, ( \
		if(x86_is_ia64(emu)) \
			x86_ia64_intercept(emu, 0); /* TODO */ \
	)) \
 \
	if(x86_selector_is_null(segment_selector)) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
	uint8_t segment_descriptor[8]; \
	x86_table_check_limit_selector(emu, segment_selector, 0, 8, X86_EXC_GP); \
	x86_descriptor_load(emu, segment_selector, segment_descriptor, X86_EXC_GP); \
 \
	if(x86_descriptor_is_system_segment(segment_descriptor) || !x86_descriptor_is_executable(segment_descriptor)) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector); \
 \
	/* TODO: assert D=1? (unless 64-bit) */ \
 \
	if(x86_descriptor_get_dpl(segment_descriptor) > cpl) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector); \
 \
	_IF_64(__size, ( \
		if(!x86_descriptor_is_size_valid(emu, segment_descriptor)) \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector); \
	)) \
 \
	if(!x86_descriptor_is_present(segment_descriptor)) \
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, segment_selector); \
 \
	unsigned new_cpl = x86_get_cpl(emu); \
	_IF_16(__size, ( \
		uoff_t new_ip = x86_descriptor_get_gate_offset_386(emu, gate_descriptor); \
	)) \
	_IF_NOT16(__size, ( \
		uoff_t new_ip = x86_descriptor_get_gate_offset_##__size(gate_descriptor); \
	)) \
	if(!x86_descriptor_is_conforming(segment_descriptor)) \
	{ \
		new_cpl = x86_descriptor_get_dpl(segment_descriptor); \
		if(new_cpl > x86_get_cpl(emu)) \
			new_cpl = x86_get_cpl(emu); \
	} \
	if(new_cpl < x86_get_cpl(emu)) \
	{ \
		/* more privilege */ \
		uint16_t old_ss = emu->sr[X86_R_SS].selector; \
		uoff_t old_rsp = emu->gpr[X86_R_SP]; \
 \
		_IF_NOT64(__size, ( \
			unsigned count = x86_descriptor_get_parameter_count(gate_descriptor); \
			uint##__size##_t parameters[count]; \
			for(unsigned i = 0; i < count; i++) \
			{ \
				parameters[i] = x86_memory_segmented_read##__size(emu, X86_R_SS, old_rsp + (__size >> 3) * i); \
			} \
		)) \
 \
		/* fetch new stack */ \
		uint16_t new_ss; \
		uoff_t new_rsp = x86_load_task_stack(emu, new_cpl, &new_ss); \
 \
		_IF_NOT64(__size, ( \
			if(x86_selector_is_null(new_ss)) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, new_ss); \
		)) \
		uint8_t stack_descriptor[8]; \
		x86_table_check_limit_selector(emu, new_ss, 0, 8, X86_EXC_GP); \
		x86_descriptor_load(emu, new_ss, stack_descriptor, X86_EXC_GP); \
 \
		_IF_NOT64(__size, ( \
			if((new_ss & X86_SEL_RPL_MASK) != new_cpl || x86_descriptor_get_dpl(stack_descriptor) != new_cpl) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, new_ss); \
 \
			if(!(!x86_descriptor_is_system_segment(stack_descriptor) \
				&& !x86_descriptor_is_executable(stack_descriptor) \
				&& x86_descriptor_is_writable(stack_descriptor))) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, new_ss); \
 \
			/* TODO: assert D=1? (unless 64-bit) */ \
 \
			if(!x86_descriptor_is_present(stack_descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, new_ss); /* TODO: is this not NP? */ \
 \
			x86_stack_descriptor_check_limit(emu, new_ss, stack_descriptor, new_rsp, (__size >> 3) * (4 + count)); \
 \
			x86_descriptor_check_limit(emu, X86_R_CS, segment_descriptor, new_ip, 1, 0); \
		)) \
 \
		_IF_64(__size, ( \
			x86_check_canonical_address(emu, X86_R_CS, new_ip, 0); \
		)) \
 \
		/* load new stack */ \
		x86_set_cpl(emu, new_cpl); \
		x86_segment_load_protected_mode(emu, X86_R_SS, new_ss, stack_descriptor); \
		emu->gpr[X86_R_SP] = new_rsp; \
 \
		/* push old stack */ \
		x86_push##__size(emu, old_ss); \
		x86_push##__size(emu, old_rsp); \
 \
		_IF_NOT64(__size, ( \
			/* push parameters */ \
			for(unsigned i = 0; i < count; i++) \
			{ \
				x86_push##__size(emu, parameters[count - i - 1]); \
			} \
		)) \
	} \
	else \
	{ \
		_IF_NOT64(__size, ( \
			x86_stack_segment_check_limit(emu, (__size >> 3) * 2, 0); \
			x86_descriptor_check_limit(emu, X86_R_CS, segment_descriptor, new_ip, 1, 0); \
		)) \
		_IF_64(__size, ( \
			x86_check_canonical_address(emu, X86_R_SS, emu->gpr[X86_R_SP] - (__size >> 3) * 2, 0); \
			x86_check_canonical_address(emu, X86_R_CS, new_ip, 0); \
		)) \
	} \
	x86_push##__size(emu, emu->sr[X86_R_CS].selector); \
	x86_push##__size(emu, emu->xip); \
	x86_segment_load_protected_mode(emu, X86_R_CS, (segment_selector & X86_SEL_INDEX_MASK) | cpl, segment_descriptor); \
	x86_set_xip(emu, new_ip); \
}

_DEFINE_x86_call_via_call_gate(16)
_DEFINE_x86_call_via_call_gate(32)
_DEFINE_x86_call_via_call_gate(64)

static inline void x86_call_via_task_gate(x86_state_t * emu, uint16_t gate_selector, uint8_t * gate_descriptor)
{
	uint16_t tss_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR);
	unsigned cpl = x86_get_cpl(emu);
	unsigned dpl = x86_descriptor_get_dpl(gate_descriptor);
	unsigned tss_rpl = tss_selector & X86_SEL_RPL_MASK;

	if(dpl < cpl || dpl < tss_rpl)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(!x86_descriptor_is_present(gate_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, gate_selector);

	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	if((tss_selector & X86_SEL_LDT) != 0)
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint8_t tss_descriptor[8];
	x86_table_check_limit_selector(emu, tss_selector, 0, 8, X86_EXC_GP);
	x86_descriptor_load(emu, tss_selector, tss_descriptor, X86_EXC_GP);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint16_t old_tss = emu->sr[X86_R_TR].selector;
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	x86_tss_busy_set(emu);
	x86_tss_set_nt(emu);
	x86_tss_set_link(emu, old_tss);
	x86_load_selectors_after_switch(emu, selector_count);
}

static inline void x86_call_via_task_segment(x86_state_t * emu, uint16_t tss_selector, uint8_t * tss_descriptor)
{
	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	unsigned cpl = x86_get_cpl(emu);
	unsigned dpl = x86_descriptor_get_dpl(tss_descriptor);
	unsigned rpl = tss_selector & X86_SEL_RPL_MASK;

	if(dpl < cpl || dpl < rpl)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	if(x86_is_ia64(emu))
		x86_ia64_intercept(emu, 0); // TODO

	uint16_t old_tss = emu->sr[X86_R_TR].selector;
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	x86_tss_busy_set(emu);
	x86_tss_set_nt(emu);
	x86_tss_set_link(emu, old_tss);
	x86_load_selectors_after_switch(emu, selector_count);
}

static inline void x86_call_via(x86_state_t * emu, uint16_t selector, uint8_t * descriptor)
{
	switch(x86_descriptor_get_type(descriptor))
	{
	case X86_DESC_TYPE_CALLGATE16:
		x86_call_via_call_gate16(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_CALLGATE32:
		if(x86_is_long_mode(emu))
			x86_call_via_call_gate64(emu, selector, descriptor);
		else
			x86_call_via_call_gate32(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_TASKGATE:
		x86_call_via_task_gate(emu, selector, descriptor);
		break;
	case X86_DESC_TYPE_TSS16_A:
	case X86_DESC_TYPE_TSS16_B:
	case X86_DESC_TYPE_TSS32_A:
	case X86_DESC_TYPE_TSS32_B:
		x86_call_via_task_segment(emu, selector, descriptor);
		break;
	default:
		assert(false);
	}
}

#define _DEFINE_x86_call_far(__size) \
static inline void x86_call_far##__size(x86_state_t * emu, uint16_t selector, uoff_t offset) \
{ \
	_IF_NOT64(__size, ( \
		if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu)) \
		{ \
			x86_stack_segment_check_limit(emu, (__size >> 3) * 2, 0); \
			_IF_16(__size, ( \
				if((offset & ~0xFFFF) != 0) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT, 0); \
			)) \
			x86_push##__size(emu, emu->sr[X86_R_CS].selector); \
			x86_push##__size(emu, emu->xip); \
			x86_segment_load_real_mode(emu, X86_R_CS, selector); \
			x86_set_xip(emu, offset); \
		} \
		else \
	)) \
	{ \
		if(x86_selector_is_null(selector)) \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
		uint8_t descriptor[16]; \
		x86_table_check_limit_selector(emu, selector, 0, 8, X86_EXC_GP); \
		x86_descriptor_load(emu, selector, descriptor, X86_EXC_GP); \
 \
		if(!x86_descriptor_is_system_segment(descriptor)) \
		{ \
			if(!x86_descriptor_is_executable(descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
 \
			/* TODO: assert D=1? (unless 64-bit) */ \
 \
			if(!x86_descriptor_is_size_valid(emu, descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
 \
			unsigned cpl = x86_get_cpl(emu); \
			unsigned dpl = x86_descriptor_get_dpl(descriptor); \
 \
			if(x86_descriptor_is_conforming(descriptor)) \
			{ \
				if(dpl > cpl) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
			} \
			else \
			{ \
				unsigned rpl = selector & X86_SEL_RPL_MASK; \
				if(rpl > cpl || dpl != cpl) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
			} \
 \
			if(!x86_descriptor_is_present(descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
 \
			x86_stack_segment_check_limit(emu, (__size >> 3) * 2, 0); \
 \
			_IF_NOT64(__size, ( \
				if(!(x86_is_long_mode(emu) && x86_descriptor_is_long(descriptor))) \
			)) \
			_IF_64(__size, ( \
				if(!x86_descriptor_is_long(descriptor)) \
			)) \
				x86_descriptor_check_limit(emu, X86_R_CS, descriptor, offset, 1, 0); \
			else \
				x86_descriptor_check_limit_64bit_mode(emu, X86_R_CS, descriptor, offset, 1, 0); \
			x86_check_canonical_address(emu, X86_R_CS, offset, 0); \
 \
			x86_push##__size(emu, emu->sr[X86_R_CS].selector); \
			x86_push##__size(emu, emu->xip); \
			x86_segment_load_protected_mode(emu, X86_R_CS, (selector & X86_SEL_INDEX_MASK) | cpl, descriptor); \
			x86_set_xip(emu, offset); \
		} \
		else \
		{ \
			_IF_NOT64(__size, ( \
				switch(x86_descriptor_get_type(descriptor)) \
				{ \
				case X86_DESC_TYPE_TSS32_A: \
				case X86_DESC_TYPE_TSS32_B: \
					_IF_16(__size, ( \
						if(emu->cpu_type < X86_CPU_386) \
							x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
					)) \
					if(x86_is_long_mode(emu)) \
						x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
					break; \
	 \
				case X86_DESC_TYPE_TASKGATE: \
					if(x86_is_long_mode(emu)) \
						x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
					break; \
	 \
				case X86_DESC_TYPE_CALLGATE32: \
					_IF_16(__size, ( \
						if(emu->cpu_type < X86_CPU_386) \
							x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
						break; \
					)) \
				case X86_DESC_TYPE_CALLGATE16: \
				case X86_DESC_TYPE_TSS16_A: \
				case X86_DESC_TYPE_TSS16_B: \
					if(x86_is_long_mode(emu) || x86_is_32bit_only(emu)) \
						x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
					break; \
				default: \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
				} \
			)) \
			_IF_64(__size, ( \
				if(x86_descriptor_get_type(descriptor) != X86_DESC_TYPE_CALLGATE32) \
				{ \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, selector); \
				} \
			)) \
 \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu)) \
			)) \
			{ \
				x86_table_check_limit_selector(emu, selector, 8, 8, X86_EXC_GP); \
				x86_descriptor_load_extension(emu, selector, descriptor); \
			} \
			x86_call_via(emu, selector, descriptor); \
		} \
	} \
}

_DEFINE_x86_call_far(16)
_DEFINE_x86_call_far(32)
_DEFINE_x86_call_far(64)

#define _DEFINE_x86_interrupt_via_gate(__size) \
static inline void x86_interrupt_via_gate##__size(x86_state_t * emu, int exception, uint8_t * gate_descriptor, bool is_interrupt_gate) \
{ \
	uint8_t segment_descriptor[8]; \
	uint16_t segment_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR); \
 \
	if(x86_selector_is_null(segment_selector)) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, (exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) == 0 ? 1 : 0); \
 \
	uoff_t error_code = (segment_selector & ~X86_SEL_RPL_MASK) | ((exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) == 0 ? 1 : 0); \
 \
	x86_table_check_limit_selector(emu, segment_selector, 0, 8, X86_EXC_GP); \
	x86_descriptor_load(emu, segment_selector, segment_descriptor, X86_EXC_GP); \
 \
	if(x86_descriptor_is_system_segment(segment_descriptor) || !x86_descriptor_is_executable(segment_descriptor)) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code); \
 \
	/* TODO: assert D=1? (unless 64-bit) */ \
 \
	unsigned dpl = x86_descriptor_get_dpl(segment_descriptor); \
	if(dpl > x86_get_cpl(emu)) \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code); \
 \
	if(!x86_descriptor_is_present(segment_descriptor)) \
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, error_code); \
 \
	unsigned cpl = x86_get_cpl(emu); \
	unsigned new_cpl = cpl; \
	if(!x86_descriptor_is_conforming(segment_descriptor)) \
	{ \
		new_cpl = dpl; \
	} \
 \
	if(emu->vm && new_cpl != 0) \
		/* Note: technically VM=1 in long mode, so checking it for 64-bit interrupts is not necessary, but we allow virtual 8086 mode in long mode as an extension */ \
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code); \
 \
	_IF_16(__size, ( \
		uoff_t offset = x86_descriptor_get_gate_offset_386(emu, gate_descriptor); \
	)) \
	_IF_NOT16(__size, ( \
		uoff_t offset = x86_descriptor_get_gate_offset_##__size(gate_descriptor); \
	)) \
 \
	_IF_NOT64(__size, ( \
		if(new_cpl < cpl) \
	)) \
	{ \
		uint16_t ss = emu->sr[X86_R_SS].selector; \
		uoff_t rsp = emu->xip; \
 \
		/* fetch new stack */ \
		_IF_64(__size, ( \
			int ist = x86_descriptor_get_ist(gate_descriptor); \
		)) \
		uint16_t new_ss; \
		uoff_t new_rsp; \
		_IF_NOT64(__size, ( \
			new_rsp = x86_load_task_stack(emu, new_cpl, &new_ss); \
		)) \
		_IF_64(__size, ( \
			if(ist != 0) \
			{ \
				new_rsp = x86_load_task_ist_stack(emu, ist); \
				new_ss = new_cpl; /* null selector */ \
			} \
			else if(new_cpl < cpl) \
			{ \
				new_rsp = x86_load_task_stack(emu, new_cpl, &new_ss); \
			} \
			else \
			{ \
				new_rsp = emu->gpr[X86_R_SP]; \
			} \
		)) \
 \
		_IF_NOT64(__size, ( \
			if(x86_selector_is_null(new_ss)) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, error_code & 1); \
 \
			if(x86_overflow(emu->sr[new_ss & 4 ? X86_R_LDTR : X86_R_GDTR].base, (new_ss & ~7) + 8, emu->sr[new_ss & X86_SEL_LDT ? X86_R_LDTR : X86_R_GDTR].limit)) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, (new_ss & ~X86_SEL_RPL_MASK) | (error_code & 1)); \
 \
			if((new_ss & X86_SEL_RPL_MASK) != new_cpl) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, (new_ss & ~X86_SEL_RPL_MASK) | (error_code & 1)); \
 \
			uint8_t stack_descriptor[8]; \
			x86_table_check_limit_selector(emu, new_ss, 0, 8, X86_EXC_GP); \
			x86_descriptor_load(emu, new_ss, stack_descriptor, X86_EXC_GP); \
 \
			if(x86_descriptor_get_dpl(stack_descriptor) != new_cpl) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, (new_ss & ~X86_SEL_RPL_MASK) | (error_code & 1)); \
 \
			if(x86_descriptor_is_system_segment(stack_descriptor) \
			|| x86_descriptor_is_executable(stack_descriptor) \
			|| !x86_descriptor_is_writable(stack_descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, (new_ss & ~X86_SEL_RPL_MASK) | (error_code & 1)); \
 \
			/* TODO: assert D=1? */ \
 \
			if(!x86_descriptor_is_present(stack_descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, (new_ss & ~X86_SEL_RPL_MASK) | (error_code & 1)); \
 \
			x86_stack_descriptor_check_limit(emu, (new_ss & ~X86_SEL_RPL_MASK) | ((error_code) & 1), stack_descriptor, new_rsp, \
				emu->vm \
					? (exception & X86_EXC_VALUE) != 0 ? (__size >> 3) * 10 : (__size >> 3) * 9 \
					: (exception & X86_EXC_VALUE) != 0 ? (__size >> 3) * 6  : (__size >> 3) * 5); \
			x86_descriptor_check_limit(emu, X86_R_CS, segment_descriptor, offset, 1, 0); \
 \
		)) \
 \
		_IF_64(__size, ( \
			x86_check_canonical_address(emu, X86_R_SS, new_rsp, error_code & 1); \
			x86_check_canonical_address(emu, X86_R_CS, offset, error_code & 1); \
		)) \
 \
		/* load new stack */ \
		x86_set_cpl(emu, new_cpl); \
		_IF_NOT64(__size, ( \
			x86_segment_load_protected_mode(emu, X86_R_SS, new_ss, stack_descriptor); \
		)) \
 \
		_IF_64(__size, ( \
			new_rsp &= ~0xF; \
		)) \
		emu->gpr[X86_R_SP] = new_rsp; \
 \
		if(emu->vm) \
		{ \
			/* Note: in 64-bit mode, this is an extension, not a scenario that can happen on commercially available x86 chips */ \
			x86_push##__size(emu, emu->sr[X86_R_GS].selector); \
			x86_push##__size(emu, emu->sr[X86_R_FS].selector); \
			x86_push##__size(emu, emu->sr[X86_R_DS].selector); \
			x86_push##__size(emu, emu->sr[X86_R_ES].selector); \
			x86_segment_load_null(emu, X86_R_ES, 0); \
			x86_segment_load_null(emu, X86_R_DS, 0); \
			x86_segment_load_null(emu, X86_R_FS, 0); \
			x86_segment_load_null(emu, X86_R_GS, 0); \
		} \
		x86_push##__size(emu, ss); \
		x86_push##__size(emu, rsp); \
	} \
	_IF_NOT64(__size, ( \
		else \
		{ \
			x86_stack_segment_check_limit(emu, (exception & X86_EXC_VALUE) != 0 ? (__size >> 3) * 4 : (__size >> 3) * 3, error_code & 1); \
			x86_descriptor_check_limit(emu, X86_R_CS, segment_descriptor, offset, 1, 0); \
		} \
	)) \
	x86_push##__size(emu, x86_flags_get16(emu)); \
	x86_push##__size(emu, emu->sr[X86_R_CS].selector); \
	x86_push##__size(emu, emu->xip); \
	emu->tf = 0; \
	emu->vm = 0; \
	emu->rf = 0; \
	emu->nt = 0; \
	if(is_interrupt_gate) \
		emu->_if = 0; \
	x86_segment_load_protected_mode(emu, X86_R_CS, (segment_selector & X86_SEL_INDEX_MASK) | cpl, segment_descriptor); \
	x86_set_xip(emu, offset); \
}

_DEFINE_x86_interrupt_via_gate(16)
_DEFINE_x86_interrupt_via_gate(32)
_DEFINE_x86_interrupt_via_gate(64)

static inline void x86_interrupt_via_task_gate(x86_state_t * emu, int exception, uint8_t * gate_descriptor)
{
	uint16_t tss_selector = x86_descriptor_get_word(gate_descriptor, X86_DESCWORD_GATE_SELECTOR);
	uoff_t error_code = (tss_selector & ~X86_SEL_RPL_MASK) | ((exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) == 0 ? 1 : 0);

	if((tss_selector & X86_SEL_LDT) != 0)
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);

	uint8_t tss_descriptor[8];
	x86_table_check_limit_selector(emu, tss_selector, 0, 8, X86_EXC_GP);
	x86_descriptor_load(emu, tss_selector, tss_descriptor, X86_EXC_GP);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);

	uint16_t old_tss = emu->sr[X86_R_TR].selector;
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	if((exception & X86_EXC_VALUE) != 0)
		x86_stack_segment_check_limit(emu, x86_is_32bit_mode(emu) ? 4 : 2, 0);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, (exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) != 0 ? 1 : 0);
	x86_tss_busy_set(emu);
	x86_tss_set_nt(emu);
	x86_tss_set_link(emu, old_tss);
	x86_load_selectors_after_switch(emu, selector_count);
}

static inline void x86_enter_interrupt(x86_state_t * emu, int exception, uoff_t error_code)
{
	emu->state = X86_STATE_RUNNING;

	x86_store_x80_registers(emu);
	if((exception & X86_EXC_FAULT) != 0)
	{
		x86_set_xip(emu, emu->old_xip);
	}
	if(x86_is_real_mode(emu))
	{
		x86_table_check_limit_exception(emu, exception & 0xFF, 4, 0);
		x86_stack_segment_check_limit(emu, 6, 0);
		x86_push16(emu, x86_flags_get16(emu));
		emu->_if = 0;
		emu->tf = 0;
		emu->md = x86_native_state_flag(emu);
		emu->ac = 0;
		x86_push16(emu, emu->sr[X86_R_CS].selector);
		x86_push16(emu, emu->xip);
		x86_segment_load_real_mode(emu, X86_R_CS, x86_memory_segmented_read16(emu, X86_R_IDTR, (exception & 0xFF) * 4 + 2));
		x86_set_xip(emu, x86_memory_segmented_read16(emu, X86_R_IDTR, (exception & 0xFF) * 4));
	}
	else
	{
		if(x86_is_virtual_8086_mode(emu) && (exception & X86_EXC_INT_N) != 0)
		{
			if((emu->cr[4] & X86_CR4_VME) == 0 && emu->iopl != 3)
			{
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
			}
			else if((emu->cr[4] & X86_CR4_VME) != 0)
			{
				/* look up interrupt redirection map in TSS */
				//x86_table_check_limit(emu, X86_R_TR, 0x66, 2, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); // TODO
				uint16_t iopb = x86_memory_segmented_read16(emu, X86_R_TR, 0x66);
				int intno = exception & 0xFF;
				//x86_table_check_limit(emu, X86_R_TR, iopb - 32 + (intno >> 3), 1, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); // TODO
				uint8_t bitmap = x86_memory_segmented_read8(emu, X86_R_TR, iopb - 32 + (intno >> 3));
				if(((bitmap >> (intno & 7)) & 1) == 0)
				{
					x86_push16(emu, x86_flags_get_image16(emu));
					x86_push16(emu, emu->sr[X86_R_CS].selector);
					x86_push16(emu, emu->xip);
					if(emu->iopl == 3)
						emu->_if = 0;
					else
						emu->vif = 0;
					emu->tf = 0;
					return;
				}
			}
		}

		uint8_t descriptor[16];
		uoff_t exception_error_code = ((exception & 0xFF) << 3) | 2 | ((exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) == 0 ? 1 : 0);
		x86_descriptor_read_exception(emu, exception & 0xFF, x86_is_long_mode(emu) ? 16 : 8, descriptor, exception_error_code);

		x86_descriptor_type_t type = x86_descriptor_get_type(descriptor);

		switch(type)
		{
		case X86_DESC_TYPE_TASKGATE:
			if(x86_is_long_mode(emu))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
			break;
		case X86_DESC_TYPE_INTGATE16:
		case X86_DESC_TYPE_TRAPGATE16:
			if(x86_is_long_mode(emu) || x86_is_32bit_only(emu))
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
			break;
		case X86_DESC_TYPE_INTGATE32:
		case X86_DESC_TYPE_TRAPGATE32:
			if(emu->cpu_type < X86_CPU_386)
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
			break;
		default:
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
		}

		if((exception & (X86_EXC_INT_N|X86_EXC_INT_SW)) != 0)
		{
			unsigned cpl = x86_get_cpl(emu);
			unsigned gate_dpl = x86_descriptor_get_dpl(descriptor);
			if(gate_dpl < cpl)
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
		}

		if(!x86_descriptor_is_present(descriptor))
		{
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, exception_error_code);
		}

		switch(type)
		{
		case X86_DESC_TYPE_TASKGATE:
			x86_interrupt_via_task_gate(emu, exception, descriptor);
			break;
		case X86_DESC_TYPE_INTGATE32:
			if(x86_is_long_mode(emu))
			{
				x86_interrupt_via_gate64(emu, exception, descriptor, true);
			}
			else
			{
				x86_interrupt_via_gate32(emu, exception, descriptor, true);
			}
			break;
		case X86_DESC_TYPE_TRAPGATE32:
			if(x86_is_long_mode(emu))
			{
				x86_interrupt_via_gate64(emu, exception, descriptor, false);
			}
			else
			{
				x86_interrupt_via_gate32(emu, exception, descriptor, false);
			}
			break;
		case X86_DESC_TYPE_INTGATE16:
			x86_interrupt_via_gate16(emu, exception, descriptor, true);
			break;
		case X86_DESC_TYPE_TRAPGATE16:
			x86_interrupt_via_gate16(emu, exception, descriptor, false);
			break;
		default:
			assert(false);
		}

		if((exception & X86_EXC_VALUE))
		{
			switch(type)
			{
			case X86_DESC_TYPE_INTGATE32:
			case X86_DESC_TYPE_TRAPGATE32:
				if(x86_is_long_mode(emu))
				{
					x86_push64(emu, error_code);
				}
				else
				{
					x86_push32(emu, error_code);
				}
				break;
			case X86_DESC_TYPE_INTGATE16:
			case X86_DESC_TYPE_TRAPGATE16:
				x86_push16(emu, error_code);
				break;
			default:
				assert(false);
			}
		}
	}
	x86_load_x80_registers(emu);
}

static inline void x86_enter_interrupt_bank_switching(x86_state_t * emu, int exception, int register_bank)
{
	emu->state = X86_STATE_RUNNING;

	x86_store_x80_registers(emu);
	if((exception & X86_EXC_FAULT) != 0)
	{
		x86_set_xip(emu, emu->old_xip);
	}

	uint16_t flags = x86_flags_get16(emu);
	x86_set_register_bank_number(emu, register_bank);
	emu->bank[emu->rb].w[X86_BANK_PSW_SAVE] = htole16(flags);
	emu->bank[emu->rb].w[X86_BANK_PC_SAVE] = htole16(emu->xip);
	emu->_if = 0;
	emu->tf = 0;
	emu->md = x86_native_state_flag(emu); // not used by V25/V55, included for consistency
	emu->ac = 0; // not used by V25/V55, included for consistency
	x86_set_xip(emu, emu->bank[emu->rb].w[X86_BANK_VECTOR_PC]);

	x86_load_x80_registers(emu);
}

static inline _Noreturn void x86_trigger_interrupt(x86_state_t * emu, int exception, uoff_t error_code)
{
	if(emu->fetch_mode == FETCH_MODE_PREFETCH)
		longjmp(emu->exc[emu->fetch_mode], 1); // simply return to the prefetch start

	if(emu->cpu_type == X86_CPU_V60)
	{
		switch(exception & 0xFF)
		{
		case X86_EXC_DE:
			x86_v60_exception(emu, V60_EXC_ZD);
			break;
		case X86_EXC_OF:
			x86_v60_exception(emu, V60_EXC_O);
			break;
		case X86_EXC_BR:
			x86_v60_exception(emu, V60_EXC_I);
			break;
		case X86_EXC_UD:
			x86_set_xip(emu, emu->old_xip + 1);
			x86_v60_exception(emu, V60_EXC_RI);
			break;
		default:
			assert(false);
		}
	}

	enum x86_exception_class_t _class = X86_EXC_CLASS_BENIGN;

	if((exception & (X86_EXC_INT_N | X86_EXC_INT_SW)) == 0)
	{
		switch(exception & 0xFF)
		{
		default:
			break;
		case X86_EXC_MP:
			if(X86_CPU_286 <= emu->cpu_type && emu->cpu_type <= X86_CPU_386)
				_class = X86_EXC_CLASS_CONTRIBUTORY;
			break;
		case X86_EXC_DE:
		case X86_EXC_TS:
		case X86_EXC_NP:
		case X86_EXC_SS:
		case X86_EXC_GP:
			_class = X86_EXC_CLASS_CONTRIBUTORY;
			break;
		case X86_EXC_CP:
			if((emu->cpu_traits.cpuid7_0.ecx & X86_CPUID7_0_ECX_CET_SS) != 0)
				_class = X86_EXC_CLASS_CONTRIBUTORY;
			break;
		case X86_EXC_PF:
			if(X86_CPU_386 <= emu->cpu_type)
				_class = X86_EXC_CLASS_PAGE_FAULT;
			break;
		case X86_EXC_VC:
			if((emu->cpu_traits.cpuid_ext31.eax & X86_CPUID_EXT31_EAX_SEV_ES) != 0)
				_class = X86_EXC_CLASS_PAGE_FAULT;
			break;
		case X86_EXC_DF:
			_class = X86_EXC_CLASS_DOUBLE_FAULT;
			break;
		}
	}

	if(_class != X86_EXC_CLASS_BENIGN)
	{
		switch(emu->current_exception)
		{
		case X86_EXC_CLASS_BENIGN:
			emu->current_exception = _class;
			break;
		case X86_EXC_CLASS_CONTRIBUTORY:
		case X86_EXC_CLASS_PAGE_FAULT:
			if(_class <= emu->current_exception)
			{
				//x86_trigger_interrupt(emu, X86_EXC_DF | X86_EXC_ABORT | X86_EXC_VALUE, 0);
				exception = X86_EXC_DF | X86_EXC_ABORT | X86_EXC_VALUE;
				error_code = 0;
				emu->current_exception = X86_EXC_CLASS_DOUBLE_FAULT;
			}
			else
			{
				emu->current_exception = _class;
			}
			break;
		case X86_EXC_CLASS_DOUBLE_FAULT:
			emu->emulation_result = X86_RESULT(X86_RESULT_TRIPLE_FAULT, 0);
			longjmp(emu->exc[emu->fetch_mode], 1);
			break;
		}
	}

	if(X86_CPU_386 <= emu->cpu_type && emu->cpu_type <= X86_CPU_486 && (exception & 0xFF) == X86_EXC_DB && (emu->dr[7] & X86_DR7_ICE) != 0)
	{
		x86_ice_storeall_386(emu, 0x60000); // TODO: unsure about address
		emu->dr[6] |= X86_DR6_SMM;
		emu->emulation_result = X86_RESULT(X86_RESULT_ICE_INTERRUPT, 0);
	}
	else
	{
		x86_enter_interrupt(emu, exception, error_code);
		emu->emulation_result = X86_RESULT(X86_RESULT_CPU_INTERRUPT, exception & 0xFF);
	}
	longjmp(emu->exc[emu->fetch_mode], 1);
	for(;;);
}

static inline void x86_return_via_nested_task(x86_state_t * emu)
{
	if(x86_overflow(emu->sr[X86_R_TR].base, 2, emu->sr[X86_R_TR].limit))
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, emu->sr[X86_R_TR].selector); // TODO: what is the exact interrupt

	uint16_t tss_selector = x86_memory_segmented_read16(emu, X86_R_TR, 0);

	if((tss_selector & X86_SEL_LDT) != 0)
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint8_t tss_descriptor[8];
	x86_table_check_limit_selector(emu, tss_selector, 0, 8, X86_EXC_GP);
	x86_descriptor_load(emu, tss_selector, tss_descriptor, X86_EXC_GP);

	switch(x86_descriptor_get_type(tss_descriptor))
	{
	case X86_DESC_TYPE_TSS16_A:
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x2B)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	case X86_DESC_TYPE_TSS32_A:
		if(emu->cpu_type < X86_CPU_386)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		if(x86_descriptor_get_limit(emu, tss_descriptor) < 0x67)
			x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	default:
		x86_trigger_interrupt(emu, X86_EXC_TS | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);
		break;
	}

	if(!x86_descriptor_is_present(tss_descriptor))
		x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, tss_selector);

	uint16_t old_tr = emu->sr[X86_R_TR].selector;
	uint8_t busy_byte = x86_tss_busy_byte(emu);
	uint32_t tss_base = emu->sr[X86_R_TR].base;
	uint16_t tss_access = emu->sr[X86_R_TR].access;
	//emu->nt = 0; // this must be executed after the task switch completed
	int selector_count = x86_switch_task(emu, tss_selector, tss_descriptor);
	x86_tss_busy_clear(emu, old_tr, busy_byte);
	x86_clear_old_nt(emu, tss_access, tss_base);
	if(x86_overflow(emu->sr[X86_R_CS].base, emu->xip, emu->sr[X86_R_CS].limit))
		x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0);
	x86_load_selectors_after_switch(emu, selector_count);
}

#define _DEFINE_x86_return_interrupt(__size) \
static inline void x86_return_interrupt##__size(x86_state_t * emu) \
{ \
	x86_store_x80_registers(emu); \
	_IF_NOT64(__size, ( \
		if(x86_is_real_mode(emu)) \
		{ \
			x86_set_xip(emu, x86_pop##__size(emu)); \
			x86_segment_load_real_mode(emu, X86_R_CS, x86_pop##__size(emu)); \
 \
			uint64_t old_flags; \
			old_flags = x86_flags_get64(emu); \
 \
			x86_flags_set##__size(emu, x86_pop##__size(emu)); \
 \
			if(!emu->md_enabled) \
				emu->md = old_flags & X86_FL_MD; \
 \
			_IF_NOT16(__size, ( \
				emu->vm = old_flags & X86_FL_VM; \
				emu->vif = old_flags & X86_FL_VIF; \
				emu->vip = old_flags & X86_FL_VIP; \
			)) \
		} \
		else if(x86_is_virtual_8086_mode(emu)) \
		{ \
			if(emu->iopl == 3) \
			{ \
				x86_set_xip(emu, x86_pop##__size(emu)); \
				x86_segment_load_real_mode(emu, X86_R_CS, x86_pop##__size(emu)); \
 \
				uint64_t old_flags; \
				old_flags = x86_flags_get64(emu); \
 \
				x86_flags_set##__size(emu, x86_pop##__size(emu)); \
 \
				emu->iopl = 3; \
				if(!emu->md_enabled) \
					emu->md = old_flags & X86_FL_MD; /* extension */ \
 \
				_IF_NOT16(__size, ( \
					emu->vm = old_flags & X86_FL_VM; \
					emu->vif = old_flags & X86_FL_VIF; \
					emu->vip = old_flags & X86_FL_VIP; \
				)) \
 \
				x86_segment_check_limit(emu, X86_R_CS, emu->xip, 1, 0); \
			} \
			else \
			{ \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
			} \
		} \
		else if(emu->nt) \
		{ \
			if(x86_is_long_mode(emu)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
			x86_return_via_nested_task(emu); \
		} \
	else \
	)) \
	{ \
		/* protected mode */ \
		_IF_NOT64(__size, ( \
			x86_segment_check_limit(emu, X86_R_SS, x86_get_stack_pointer(emu), (__size >> 3) * 3, 0); \
		)) \
		_IF_NOT64(__size, ( \
			if(x86_is_long_mode(emu)) \
		)) \
			x86_check_canonical_address(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 3, 0); \
 \
		uoff_t rip = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu)); \
		uint16_t cs = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3)); \
		uint64_t flags = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 2); \
 \
		_IF_NOT16(__size, ( \
			_IF_NOT64(__size, ( \
				if((!x86_is_long_mode(emu) || x86_is_long_vm86_supported(emu)) && ((flags & X86_FL_VM) != 0 && x86_get_cpl(emu) == 0)) \
			)) \
			_IF_64(__size, ( \
				if(x86_is_long_vm86_supported(emu) && ((flags & X86_FL_VM) != 0 && x86_get_cpl(emu) == 0)) \
			)) \
			{ \
				/* return to virtual 8086 mode */ \
				/* Note: in long mode, this is an extension */ \
				_IF_NOT64(__size, ( \
					x86_segment_check_limit(emu, X86_R_SS, x86_get_stack_pointer(emu), (__size >> 3) * 9, 0); \
					x86_segment_check_limit(emu, X86_R_CS, rip, 1, 0); \
				)) \
				x86_set_xip(emu, rip); \
				x86_segment_load_real_mode_full(emu, X86_R_CS, cs); \
				x86_flags_set##__size(emu, flags); \
				uoff_t esp = x86_pop##__size(emu); \
				uint16_t ss = x86_pop##__size(emu); \
				x86_segment_load_real_mode_full(emu, X86_R_ES, x86_pop##__size(emu)); \
				x86_segment_load_real_mode_full(emu, X86_R_DS, x86_pop##__size(emu)); \
				x86_segment_load_real_mode_full(emu, X86_R_FS, x86_pop##__size(emu)); \
				x86_segment_load_real_mode_full(emu, X86_R_GS, x86_pop##__size(emu)); \
				x86_segment_load_real_mode_full(emu, X86_R_SS, ss); \
				emu->gpr[X86_R_SP] = esp; \
				x86_set_cpl(emu, 3); \
			} \
			else \
		)) \
		{ \
			/* TODO: no documentation found for 64-bit, check */ \
			bool restore_stack = false; \
			bool outer_privilege = false; \
			uint16_t ss; \
			uoff_t rsp; \
 \
			if((cs & X86_SEL_RPL_MASK) > x86_get_cpl(emu)) \
			{ \
				outer_privilege = restore_stack = true; \
			} \
			else if(x86_is_64bit_mode(emu)) \
			{ \
				restore_stack = true; \
			} \
 \
			if(restore_stack) \
			{ \
				_IF_NOT64(__size, ( \
					x86_segment_check_limit(emu, X86_R_SS, x86_get_stack_pointer(emu), (__size >> 3) * 5, 0); \
				)) \
				_IF_NOT64(__size, ( \
					if(x86_is_long_mode(emu)) \
				)) \
					x86_check_canonical_address(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 5, 0); \
				rsp = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 3); \
				ss = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 4); \
			} \
 \
			if(x86_selector_is_null(cs)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
			uint8_t descriptor[8]; \
			x86_table_check_limit_selector(emu, cs, 0, 8, X86_EXC_GP); \
			/* TODO: check canonical in long mode */ \
			x86_descriptor_load(emu, cs, descriptor, X86_EXC_GP); \
 \
			if(x86_descriptor_is_system_segment(descriptor) || !x86_descriptor_is_executable(descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
			/* TODO: assert D=1? (unless 64-bit) */ \
 \
			if(!x86_descriptor_is_size_valid(emu, descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
			if(x86_descriptor_is_conforming(descriptor)) \
			{ \
				if(x86_descriptor_get_dpl(descriptor) > (cs & X86_SEL_RPL_MASK)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
			} \
			else \
			{ \
				if(x86_descriptor_get_dpl(descriptor) != (cs & X86_SEL_RPL_MASK)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
			} \
 \
			if(!x86_descriptor_is_present(descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
			uint8_t stack_descriptor[8]; \
 \
			if(restore_stack) \
			{ \
				_IF_NOT64(__size, ( \
					if(x86_selector_is_null(ss) && !(x86_is_long_mode(emu) && x86_descriptor_is_long(descriptor))) /* TODO: what happens in 64-bit */ \
				)) \
				_IF_64(__size, ( \
					if(x86_selector_is_null(ss) && !(x86_descriptor_is_long(descriptor))) /* TODO: what happens in 64-bit */ \
				)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
				_IF_NOT64(__size, ( \
					if(x86_is_long_mode(emu) && ss == 3) /* TODO: is this needed */ \
				)) \
				_IF_64(__size, ( \
					if(ss == 3) /* TODO: is this needed */ \
				)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
				x86_table_check_limit_selector(emu, ss, 0, 8, X86_EXC_GP); \
				/* TODO: check canonical in long mode */ \
				x86_descriptor_load(emu, ss, stack_descriptor, X86_EXC_GP); \
 \
				if((ss & X86_SEL_RPL_MASK) != (cs & X86_SEL_RPL_MASK)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
				if(x86_descriptor_is_system_segment(stack_descriptor) \
				|| x86_descriptor_is_executable(stack_descriptor) \
				|| !x86_descriptor_is_writable(stack_descriptor)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
				/* TODO: assert D=1? (unless 64-bit) */ \
 \
				if(x86_descriptor_get_dpl(stack_descriptor) != (cs & 3)) \
					x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
				if(!x86_descriptor_is_present(stack_descriptor)) \
					x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
			} \
 \
			_IF_NOT64(__size, ( \
				x86_descriptor_check_limit(emu, cs, descriptor, rip, 1, 0); \
			)) \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu)) \
			)) \
				x86_check_canonical_address(emu, X86_R_CS, rip, 0); \
 \
			if(x86_get_cpl(emu) > emu->iopl) \
			{ \
				flags = (flags & ~X86_FL_IF) | emu->_if; \
			} \
			if(x86_get_cpl(emu) != 0) \
			{ \
				flags = (flags & ~X86_FL_IOPL_MASK) | (emu->iopl << X86_FL_IOPL_SHIFT); \
				_IF_NOT16(__size, ( \
					flags = (flags & ~X86_FL_VM) | emu->vm; \
					flags = (flags & ~X86_FL_VIF) | emu->vif; \
					flags = (flags & ~X86_FL_VIP) | emu->vip; \
				)) \
			} \
 \
			x86_flags_set##__size(emu, flags); \
			x86_set_xip(emu, rip); \
			x86_segment_load_protected_mode(emu, X86_R_CS, cs, descriptor); \
 \
			x86_set_cpl(emu, cs); \
 \
			if(restore_stack) \
			{ \
				emu->gpr[X86_R_SP] = rsp; \
				x86_segment_load_protected_mode(emu, X86_R_SS, ss, stack_descriptor); \
			} \
			else \
			{ \
				x86_stack_adjust(emu, 6); \
			} \
 \
			if(outer_privilege) \
			{ \
				_IF_NOT64(__size, ( \
					if(x86_is_long_mode(emu) || (cs & 3) > x86_get_cpl(emu)) \
				)) \
				{ \
					for(int i = 0; i <= 8; i++) \
					{ \
						if(i == X86_R_CS || i == X86_R_SS) \
							continue; \
						if(x86_access_get_dpl(emu->sr[i].access) < x86_get_cpl(emu) && (!x86_access_is_executable(emu->sr[i].access) || !x86_access_is_conforming(emu->sr[i].access))) \
							x86_segment_load_null(emu, i, 0); \
					} \
				} \
			} \
		} \
	} \
	x86_load_x80_registers(emu); \
}

_DEFINE_x86_return_interrupt(16)
_DEFINE_x86_return_interrupt(32)
_DEFINE_x86_return_interrupt(64)

#define _DEFINE_x86_return_far(__size) \
static inline void x86_return_far##__size(x86_state_t * emu, unsigned bytes) \
{ \
	_IF_NOT64(__size, ( \
		if(x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu)) \
		{ \
			x86_segment_check_limit(emu, X86_R_SS, emu->gpr[X86_R_SP] & ((1L << __size) - 1), (__size >> 3) * 2, 0); \
			x86_set_xip(emu, x86_pop##__size(emu)); \
			x86_segment_load_real_mode(emu, X86_R_CS, x86_pop##__size(emu)); \
			_IF_16(__size, ( /* TODO: why not for 32-bit returns? */ \
				x86_segment_check_limit(emu, X86_R_CS, emu->xip, 1, 0); \
			)) \
		} \
		else \
	)) \
	{ \
		/* protected mode */ \
		_IF_NOT64(__size, ( \
			x86_segment_check_limit(emu, X86_R_SS, x86_get_stack_pointer(emu), (__size >> 3) * 2, 0); \
		)) \
		_IF_NOT64(__size, ( \
			if(x86_is_long_mode(emu)) \
		)) \
			x86_check_canonical_address(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 2, 0); \
		uoff_t rip = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu)); \
		uint16_t cs = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3)); \
 \
		_IF_NOT64(__size, ( \
			if(x86_selector_is_null(cs)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
		)) \
 \
		uint8_t descriptor[8]; \
		x86_table_check_limit_selector(emu, cs, 0, 8, X86_EXC_GP); \
		/* TODO: check canonical in long mode */ \
		x86_descriptor_load(emu, cs, descriptor, X86_EXC_GP); \
 \
		if(x86_descriptor_is_system_segment(descriptor) || !x86_descriptor_is_executable(descriptor)) \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
		/* TODO: assert D=1? (unless 64-bit) */ \
 \
		if(!x86_descriptor_is_size_valid(emu, descriptor)) \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
		if((cs & X86_SEL_RPL_MASK) < x86_get_cpl(emu)) \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
		if(x86_descriptor_is_conforming(descriptor)) \
		{ \
			if(x86_descriptor_get_dpl(descriptor) > (cs & 3)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
		} \
		else \
		{ \
			if(x86_descriptor_get_dpl(descriptor) != (cs & 3)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
		} \
 \
		if(!x86_descriptor_is_present(descriptor)) \
			x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, cs); \
 \
		if((cs & X86_SEL_RPL_MASK) > x86_get_cpl(emu)) \
		{ \
			/* return to outer privilege */ \
 \
			_IF_NOT64(__size, ( \
				x86_segment_check_limit(emu, X86_R_SS, x86_get_stack_pointer(emu), (__size >> 3) * 4 + bytes, 0); \
			)) \
 \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu)) \
			)) \
				x86_check_canonical_address(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 4 + bytes, 0); \
 \
			uoff_t rsp = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 2 + bytes); \
			uint16_t ss = x86_memory_segmented_read##__size(emu, X86_R_SS, x86_get_stack_pointer(emu) + (__size >> 3) * 3 + bytes); \
 \
			_IF_NOT64(__size, ( \
				if(x86_selector_is_null(ss) && !(x86_is_long_mode(emu) && x86_descriptor_is_long(descriptor))) \
			)) \
			_IF_64(__size, ( \
				if(x86_selector_is_null(ss) && !(x86_descriptor_is_long(descriptor))) \
			)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu) && ss == 3) \
			)) \
			_IF_64(__size, ( \
				if(ss == 3) \
			)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
 \
			uint8_t stack_descriptor[8]; \
			x86_table_check_limit_selector(emu, ss, 0, 8, X86_EXC_GP); \
			/* TODO: check canonical in long mode */ \
			x86_descriptor_load(emu, ss, stack_descriptor, X86_EXC_GP); \
 \
			if((ss & X86_SEL_RPL_MASK) != (cs & X86_SEL_RPL_MASK)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
			if(x86_descriptor_is_system_segment(stack_descriptor) \
			|| x86_descriptor_is_executable(stack_descriptor) \
			|| !x86_descriptor_is_writable(stack_descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
			/* TODO: assert D=1? (unless 64-bit) */ \
 \
			if(x86_descriptor_get_dpl(stack_descriptor) != (cs & 3)) \
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
			if(!x86_descriptor_is_present(stack_descriptor)) \
				x86_trigger_interrupt(emu, X86_EXC_NP | X86_EXC_FAULT | X86_EXC_VALUE, ss); \
 \
			x86_descriptor_check_limit(emu, cs, descriptor, rip, 1, 0); /* TODO: only if not 64-bit */ \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu)) \
			)) \
				x86_check_canonical_address(emu, X86_R_CS, rip, 0); \
 \
			x86_set_xip(emu, rip); \
			x86_segment_load_protected_mode(emu, X86_R_CS, cs, descriptor); \
			x86_set_cpl(emu, cs & X86_SEL_RPL_MASK); \
 \
			emu->gpr[X86_R_SP] = rsp; \
			x86_segment_load_protected_mode(emu, X86_R_CS, ss, stack_descriptor); \
 \
			for(int i = 0; i <= 8; i++) \
			{ \
				if(i == X86_R_CS || i == X86_R_SS) \
					continue; \
				if(x86_access_get_dpl(emu->sr[i].access) < x86_get_cpl(emu) && (!x86_access_is_executable(emu->sr[i].access) || !x86_access_is_conforming(emu->sr[i].access))) \
					x86_segment_load_null(emu, i, 0); \
			} \
		} \
		else \
		{ \
			/* return to same privilege level */ \
			x86_descriptor_check_limit(emu, cs, descriptor, rip, 1, 0);  /* TODO: only if not 64-bit */ \
			_IF_NOT64(__size, ( \
				if(x86_is_long_mode(emu)) \
			)) \
				x86_check_canonical_address(emu, X86_R_CS, rip, 0); \
 \
			x86_stack_adjust(emu, (__size >> 3) * 2); \
			x86_set_xip(emu, rip); \
			x86_segment_load_protected_mode(emu, X86_R_CS, cs, descriptor); \
		} \
	} \
}

_DEFINE_x86_return_far(16)
_DEFINE_x86_return_far(32)
_DEFINE_x86_return_far(64)



// Provides the main instructions to the CPU

static inline void x86_jump(x86_state_t * emu, uaddr_t value)
{
	x86_segment_check_limit(emu, X86_R_CS, value, 1, 0);
	x86_check_canonical_address(emu, X86_R_CS, value, 0);
	emu->xip = value;
}

static inline void x86_advance_ip(x86_state_t * emu, size_t count)
{
	switch(x86_get_code_size(emu))
	{
	case SIZE_8BIT:
	case SIZE_16BIT:
		emu->xip = (emu->xip + count) & 0xFFFF;
		break;
	case SIZE_32BIT:
		emu->xip = (emu->xip + count) & 0xFFFFFFFF;
		break;
	case SIZE_64BIT:
		emu->xip += count;
		break;
	default:
		assert(false);
	}
}

static inline uint8_t x86_fetch8(x86_parser_t * prs, x86_state_t * emu)
{
	if(emu != NULL)
	{
		uoff_t ip = emu->xip;

		x86_advance_ip(emu, 1);

		return x86_memory_segmented_read8_exec(emu, X86_R_CS, ip);
	}
	else
	{
		return prs->fetch8(prs);
	}
}

static inline uint16_t x86_fetch16(x86_parser_t * prs, x86_state_t * emu)
{
	if(emu != NULL)
	{
		uoff_t ip = emu->xip;

		// TODO: wrap around
		x86_advance_ip(emu, 2);

		return x86_memory_segmented_read16_exec(emu, X86_R_CS, ip);
	}
	else
	{
		return prs->fetch16(prs);
	}
}

static inline uint32_t x86_fetch32(x86_parser_t * prs, x86_state_t * emu)
{
	if(emu != NULL)
	{
		uoff_t ip = emu->xip;

		// TODO: wrap around
		x86_advance_ip(emu, 4);

		return x86_memory_segmented_read32_exec(emu, X86_R_CS, ip);
	}
	else
	{
		return prs->fetch32(prs);
	}
}

static inline uint64_t x86_fetch64(x86_parser_t * prs, x86_state_t * emu)
{
	if(emu != NULL)
	{
		uoff_t ip = emu->xip;

		// TODO: wrap around
		x86_advance_ip(emu, 4);

		return x86_memory_segmented_read32_exec(emu, X86_R_CS, ip);
	}
	else
	{
		return prs->fetch64(prs);
	}
}

static inline uoff_t x86_fetch_addrsize(x86_parser_t * prs, x86_state_t * emu)
{
	switch(prs->address_size)
	{
	case SIZE_16BIT:
		return x86_fetch16(prs, emu);
	case SIZE_32BIT:
		return x86_fetch32(prs, emu);
	case SIZE_64BIT:
		return x86_fetch64(prs, emu);
	default:
		assert(false);
	}
}

static inline uoff_t x86_get_stack_pointer(x86_state_t * emu)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		return x86_register_get16(emu, X86_R_SP);
	case SIZE_32BIT:
		return x86_register_get32(emu, X86_R_SP);
	case SIZE_64BIT:
		return x86_register_get64(emu, X86_R_SP);
	default:
		assert(false);
	}
}

static inline void x86_stack_adjust(x86_state_t * emu, uoff_t value)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			sp += value;
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			esp += value;
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			rsp += value;
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
}

/* 8086 undefined */
static inline void x86_push8(x86_state_t * emu, uint16_t value)
{
	uint16_t sp = x86_register_get16(emu, X86_R_SP);
	// SP is decremented by 2, but only a single byte is written
	sp -= 2;
	x86_memory_segmented_write8(emu, X86_R_SS, sp, value);
	x86_register_set16(emu, X86_R_SP, sp);
}

static inline void x86_push16(x86_state_t * emu, uint16_t value)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			sp -= 2;
			x86_memory_segmented_write16(emu, X86_R_SS, sp, value);
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			esp -= 2;
			x86_memory_segmented_write16(emu, X86_R_SS, esp, value);
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			rsp -= 2;
			x86_memory_segmented_write16(emu, X86_R_SS, rsp, value);
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
}

static inline uint16_t x86_pop16(x86_state_t * emu)
{
	uint16_t value;
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			value = x86_memory_segmented_read16(emu, X86_R_SS, sp);
			sp += 2;
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			value = x86_memory_segmented_read16(emu, X86_R_SS, esp);
			esp += 2;
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			value = x86_memory_segmented_read16(emu, X86_R_SS, rsp);
			rsp += 2;
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
	return value;
}

static inline void x86_push32(x86_state_t * emu, uint32_t value)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			sp -= 4;
			x86_memory_segmented_write32(emu, X86_R_SS, sp, value);
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			esp -= 4;
			x86_memory_segmented_write32(emu, X86_R_SS, esp, value);
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			rsp -= 4;
			x86_memory_segmented_write32(emu, X86_R_SS, rsp, value);
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
}

static inline uint32_t x86_pop32(x86_state_t * emu)
{
	uint32_t value;
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			value = x86_memory_segmented_read32(emu, X86_R_SS, sp);
			sp += 4;
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			value = x86_memory_segmented_read32(emu, X86_R_SS, esp);
			esp += 4;
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			value = x86_memory_segmented_read32(emu, X86_R_SS, rsp);
			rsp += 4;
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
	return value;
}

static inline void x86_push64(x86_state_t * emu, uint64_t value)
{
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			sp -= 8;
			x86_memory_segmented_write64(emu, X86_R_SS, sp, value);
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			esp -= 8;
			x86_memory_segmented_write64(emu, X86_R_SS, esp, value);
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			rsp -= 8;
			x86_memory_segmented_write64(emu, X86_R_SS, rsp, value);
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
}

static inline uint64_t x86_pop64(x86_state_t * emu)
{
	uint64_t value;
	switch(x86_get_stack_size(emu))
	{
	case SIZE_16BIT:
		{
			uint16_t sp = x86_register_get16(emu, X86_R_SP);
			value = x86_memory_segmented_read64(emu, X86_R_SS, sp);
			sp += 8;
			x86_register_set16(emu, X86_R_SP, sp);
		}
		break;
	case SIZE_32BIT:
		{
			uint32_t esp = x86_register_get32(emu, X86_R_SP);
			value = x86_memory_segmented_read64(emu, X86_R_SS, esp);
			esp += 8;
			x86_register_set32(emu, X86_R_SP, esp);
		}
		break;
	case SIZE_64BIT:
		{
			uint64_t rsp = x86_register_get64(emu, X86_R_SP);
			value = x86_memory_segmented_read64(emu, X86_R_SS, rsp);
			rsp += 8;
			x86_register_set64(emu, X86_R_SP, rsp);
		}
		break;
	default:
		assert(false);
	}
	return value;
}

static inline uint16_t x86_bswap16(uint16_t x)
{
	return
		((x >> 8) & 0x00FF) |
		((x << 8) & 0xFF00);
}

static inline uint32_t x86_bswap32(uint32_t x)
{
	return
		((x >> 24) & 0x000000FF) |
		((x >>  8) & 0x0000FF00) |
		((x <<  8) & 0x00FF0000) |
		((x << 24) & 0xFF000000);
}

static inline uint64_t x86_bswap64(uint64_t x)
{
	return
		((x >> 56) & 0x00000000000000FF) |
		((x >> 40) & 0x000000000000FF00) |
		((x >> 24) & 0x0000000000FF0000) |
		((x >>  8) & 0x00000000FF000000) |
		((x <<  8) & 0x000000FF00000000) |
		((x << 24) & 0x0000FF0000000000) |
		((x << 40) & 0x00FF000000000000) |
		((x << 56) & 0xFF00000000000000);
}

static inline uint32_t x86_cpuid_highest_standard_function(x86_state_t * emu)
{
	return emu->cpu_traits.cpuid0.eax;
}

static inline uint32_t x86_cpuid_highest_extended_function(x86_state_t * emu)
{
	return emu->cpu_traits.cpuid_ext0.eax;
}

static inline void x86_cpuid(x86_state_t * emu)
{
	uint32_t eax = x86_register_get32(emu, X86_R_AX);
	if(eax < 0x80000000 ? eax > x86_cpuid_highest_standard_function(emu) : eax > x86_cpuid_highest_extended_function(emu))
	{
		eax = x86_cpuid_highest_standard_function(emu);
	}
	switch(eax)
	{
	case 0:
		emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid0.eax;
		emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid0.ebx;
		emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid0.ecx;
		emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid0.edx;
		break;
	case 1:
		emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid1.eax;
		emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid1.ebx;
		emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid1.ecx;
		emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid1.edx;
		break;
		/* TODO */

	case 7:
		{
			uint32_t ecx = x86_register_get32(emu, X86_R_CX);
			switch(ecx)
			{
			case 0:
				emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid7_0.eax;
				emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid7_0.ebx;
				emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid7_0.ecx;
				emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid7_0.edx;
				break;
			case 1:
				emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid7_1.eax;
				emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid7_1.ebx;
				emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid7_1.ecx;
				emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid7_1.edx;
				break;
			}
		}
		break;

	case 0x80000000:
		emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid_ext0.eax;
		emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid_ext0.ebx;
		emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid_ext0.ecx;
		emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid_ext0.edx;
		break;
	case 0x80000001:
		emu->gpr[X86_R_AX] = emu->cpu_traits.cpuid_ext1.eax;
		emu->gpr[X86_R_BX] = emu->cpu_traits.cpuid_ext1.ebx;
		emu->gpr[X86_R_CX] = emu->cpu_traits.cpuid_ext1.ecx;
		emu->gpr[X86_R_DX] = emu->cpu_traits.cpuid_ext1.edx; // TODO: some flags should be copied over from emu->cpu_traits.cpuid1[2] if running AMD
		break;
		/* TODO */
	}
}

static inline uint8_t x86_translate_opcode(x86_parser_t * prs, uint8_t opcode)
{
	if(prs->cpu_type == X86_CPU_V25 && prs->cpu_traits.cpu_subtype == X86_CPU_V25_V25S)
		return (* prs->opcode_translation_table)[opcode];
	else
		return opcode;
}

// used for NEC INS and 80386B0 IBTS
static inline void x86_bitfield_insert16(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, unsigned offset, unsigned length, uint16_t value)
{
	uint8_t buffer[3];
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
	uint16_t mask = (1 << length) - 1;
	value &= mask;
	buffer[0] = (buffer[0] & ~(mask << offset)) | (value << offset);
	if(offset + length > 8)
	{
		buffer[1] = (buffer[1] & ~(mask >> (8 - offset))) | (value >> (8 - offset));
		if(offset + length > 16)
		{
			buffer[2] = (buffer[2] & ~(mask >> (16 - offset))) | (value >> (16 - offset));
		}
	}
	// TODO: check rights
	x86_memory_segmented_write(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
}

// used for 80386B0 IBTS
static inline void x86_bitfield_insert32(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, unsigned offset, unsigned length, uint32_t value)
{
	uint8_t buffer[5];
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
	uint32_t mask = ((uint32_t)1 << length) - 1;
	value &= mask;
	buffer[0] = (buffer[0] & ~(mask << offset)) | (value << offset);
	for(unsigned i = 1; i <= 2 && offset + length > 8 * i; i++)
	{
		buffer[i] = (buffer[i] & ~(mask >> (8 * i - offset))) | (value >> (8 * i - offset));
	}
	// TODO: check rights
	x86_memory_segmented_write(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
}

// used for NEC EXT and 80386B0 XBTS
static inline uint16_t x86_bitfield_extract16(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, unsigned offset, unsigned length)
{
	uint8_t buffer[3];
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
	uint16_t mask = (1 << length) - 1;
	uint16_t value = buffer[0] >> offset;
	if(offset + length > 8)
	{
		value |= buffer[1] << (8 - offset);
		if(offset + length > 16)
		{
			value |= buffer[2] << (16 - offset);
		}
	}
	return value & mask;
}

// used for 80386B0 XBTS
static inline uint32_t x86_bitfield_extract32(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, unsigned offset, unsigned length)
{
	uint8_t buffer[3];
	// TODO: check rights
	x86_memory_segmented_read(emu, segment_number, address, (offset + length + 7) >> 3, buffer);
	uint32_t mask = ((uint32_t)1 << length) - 1;
	uint32_t value = buffer[0] >> offset;
	for(unsigned i = 1; i <= 2 && offset + length > 8 * i; i++)
	{
		value |= buffer[i] << (8 * i - offset);
	}
	return value & mask;
}

// used for NEC V55
static inline void x86_queue_head_out(x86_state_t * emu, uint16_t parameter_table[4])
{
	uint32_t queue_address = ((uint32_t)parameter_table[1] << 4) + parameter_table[0];
	uint16_t queue_head = x86_memory_read16(emu, queue_address);
	if(queue_head == 0)
	{
		// queue is empty
		emu->zf = X86_FL_ZF;
	}
	else
	{
		// store queue head
		parameter_table[2] = queue_head;
		if(queue_head == x86_memory_read16(emu, queue_address + 2))
		{
			// clear queue
			x86_memory_write16(emu, queue_address, 0);
		}
		else
		{
			// make new queue head
			x86_memory_write16(emu, queue_address, x86_memory_read16(emu, ((uint32_t)queue_head << 4) + parameter_table[3]));
		}
		emu->zf = 0;
	}
}

// used for NEC V55
static inline void x86_queue_out(x86_state_t * emu, uint16_t parameter_table[4])
{
	uint32_t queue_address = ((uint32_t)parameter_table[1] << 4) + parameter_table[0];
	uint16_t queue_head = x86_memory_read16(emu, queue_address);
	if(queue_head == 0)
	{
		// queue is empty
		emu->zf = X86_FL_ZF;
	}
	else
	{
		uint16_t queue_tail = x86_memory_read16(emu, queue_address + 2);
		if(queue_head == parameter_table[2])
		{
			// head is link to remove
			if(queue_head == queue_tail)
			{
				// clear queue
				x86_memory_write16(emu, queue_address, 0);
			}
			else
			{
				// make new queue head
				x86_memory_write16(emu, queue_address, x86_memory_read16(emu, ((uint32_t)queue_head << 4) + parameter_table[3]));
			}
		}
		else
		{
			uint16_t current_link = queue_head;
			for(;;)
			{
				uint16_t next_link = x86_memory_read16(emu, ((uint32_t)current_link << 4) + parameter_table[3]);
				if(next_link == parameter_table[2])
				{
					// chain to following link
					x86_memory_write16(emu, ((uint32_t)current_link << 4) + parameter_table[3],
						x86_memory_read16(emu, ((uint32_t)next_link << 4) + parameter_table[3]));
					if(next_link == queue_tail)
					{
						// reset tail
						x86_memory_write16(emu, queue_address + 2, current_link);
					}
					else
					{
						// chain back to current link
						uint16_t following_link = x86_memory_read16(emu, ((uint32_t)next_link << 4) + parameter_table[3]);
						x86_memory_write16(emu, ((uint32_t)following_link << 4) + parameter_table[3] + 2,
							x86_memory_read16(emu, ((uint32_t)current_link << 4) + parameter_table[3]));
					}
				}
				if(current_link == queue_tail)
					break; // link not found
				current_link = next_link;
			}
		}
		emu->zf = 0;
	}
}

// used for NEC V55
static inline void x86_queue_tail_in(x86_state_t * emu, uint16_t parameter_table[4])
{
	uint32_t queue_address = ((uint32_t)parameter_table[1] << 4) + parameter_table[0];
	uint16_t queue_head = x86_memory_read16(emu, queue_address);
	if(queue_head == 0)
	{
		// set head
		x86_memory_write16(emu, queue_address, parameter_table[2]);
	}
	else
	{
		uint16_t queue_tail = x86_memory_read16(emu, queue_address + 2);
		// chain forward from previous tail
		x86_memory_write16(emu, ((uint32_t)queue_tail << 4) + parameter_table[3], parameter_table[2]);
		// chain back from new tail
		x86_memory_write16(emu, ((uint32_t)parameter_table[2] << 4) + parameter_table[3] + 2, queue_tail);
	}
	// set tail
	x86_memory_write16(emu, queue_address + 2, parameter_table[2]);
}


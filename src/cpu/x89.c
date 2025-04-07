
// Intel 8089 emulation

static inline x89_address_t x89_base_register_get(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg)
{
	switch(reg & 3)
	{
	case 0:
		return emu->x89.channel[channel_number].r[X89_R_GA];
	case 1:
		return emu->x89.channel[channel_number].r[X89_R_GB];
	case 2:
		return emu->x89.channel[channel_number].r[X89_R_GC];
	case 3:
		return (x89_address_t)
			{
				.address = emu->x89.channel[channel_number].pp,
				.tag = 0
			};
	default:
		assert(false);
	}
}

static inline uint16_t x89_register_get16(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg)
{
	return emu->x89.channel[channel_number].r[reg & 7].address;
}

static inline void x89_register_set16(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg, uint16_t value)
{
	// top 4 bits are undefined for 20-bit operands
	emu->x89.channel[channel_number].r[reg & 7].address = (int16_t)value;
}

static inline void x89_register_set16_local(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg, uint16_t value)
{
	emu->x89.channel[channel_number].r[reg & 7].address = (int16_t)value;
	switch((reg & 7))
	{
	case X89_R_GA:
	case X89_R_GB:
	case X89_R_GC:
	case X89_R_TP:
		emu->x89.channel[channel_number].r[reg & 7].tag = 1;
	}
}

static inline uint32_t x89_register_get32(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg)
{
	switch((reg & 7))
	{
	case X89_R_GA:
	case X89_R_GB:
	case X89_R_GC:
	case X89_R_TP:
		return emu->x89.channel[channel_number].r[reg & 7].address & 0xFFFFF;
	default:
		return (uint16_t)emu->x89.channel[channel_number].r[reg & 7].address;
	}
}

static inline void x89_register_set32(x86_state_t * emu, unsigned channel_number, x89_regnum_t reg, uint32_t value)
{
	switch((reg & 7))
	{
	case X89_R_GA:
	case X89_R_GB:
	case X89_R_GC:
	case X89_R_TP:
		emu->x89.channel[channel_number].r[reg & 7].address = value & 0xFFFFF;
		break;
	default:
		emu->x89.channel[channel_number].r[reg & 7].address = (int16_t)value;
		break;
	}
}

static inline void x89_channel_busy_clear(x86_state_t * emu, unsigned channel_number)
{
	x86_memory_write8_external(emu, emu->x89.cp + 8 * channel_number, 0x00);
	emu->x89.channel[channel_number].running = false;
}

static inline void x89_channel_busy_set(x86_state_t * emu, unsigned channel_number)
{
	x86_memory_write8_external(emu, emu->x89.cp + 8 * channel_number, 0xFF);
	emu->x89.channel[channel_number].running = true;
}

static inline void x89_channel_attention(x86_state_t * emu)
{
	if(!emu->x89.initialized)
	{
		uaddr_t scb;
		emu->x89.sysbus = x86_memory_read16_external(emu, 0xFFFF6);
		scb = x86_memory_read16_external(emu, 0xFFFF8);
		scb += (uint32_t)x86_memory_read16_external(emu, 0xFFFFA) << 4;

		emu->x89.soc = x86_memory_read16_external(emu, scb);
		emu->x89.cp = x86_memory_read16_external(emu, scb + 2);
		emu->x89.cp += (uint32_t)x86_memory_read16_external(emu, scb + 4) << 4;

		/*for(int i = 0; i < 2; i++)
		{
			x89_channel_busy_clear(emu, i);
		}*/
		x89_channel_busy_clear(emu, 0);

		emu->x89.initialized = true;
	}
	else
	{
		for(int channel_number = 0; channel_number < 2; channel_number++)
		{
			uint8_t ccw = x86_memory_read8_external(emu, emu->x89.cp + 8 * channel_number);
			uint16_t pb_offset, pb_segment;
			uint16_t tb_offset, tb_segment;
			uint8_t data[4];
			switch(ccw & 7)
			{
			case 0:
				break;
			case 1:
				// start channel in local space
				pb_offset = x86_memory_read16_external(emu, emu->x89.cp + 8 * channel_number + 2);
				pb_segment = x86_memory_read16_external(emu, emu->x89.cp + 8 * channel_number + 4);
				emu->x89.channel[channel_number].pp = ((uint32_t)pb_segment << 4) + pb_offset;
				tb_offset = x86_memory_read16_external(emu, emu->x89.channel[channel_number].pp);
				emu->x89.channel[channel_number].r[X89_R_TP].address = tb_offset;
				emu->x89.channel[channel_number].r[X89_R_TP].tag = 1;
				x89_channel_busy_set(emu, channel_number);
				break;
			case 3:
				// start channel in system space
				pb_offset = x86_memory_read16_external(emu, emu->x89.cp + 8 * channel_number + 2);
				pb_segment = x86_memory_read16_external(emu, emu->x89.cp + 8 * channel_number + 4);
				emu->x89.channel[channel_number].pp = ((uint32_t)pb_segment << 4) + pb_offset;
				tb_offset = x86_memory_read16_external(emu, emu->x89.channel[channel_number].pp);
				tb_segment = x86_memory_read16_external(emu, emu->x89.channel[channel_number].pp + 2);
				emu->x89.channel[channel_number].r[X89_R_TP].address = ((uint32_t)tb_segment << 4) + tb_offset;
				emu->x89.channel[channel_number].r[X89_R_TP].tag = 0;
				x89_channel_busy_set(emu, channel_number);
				break;
			case 5:
				// continue channel, reload tp/psw from PSW
				x86_memory_read_external(emu, emu->x89.channel[channel_number].pp, 4, data);
				emu->x89.channel[channel_number].r[X89_R_TP].address = data[0] | (data[1] << 8) | (((uint32_t)data[2] & 0xF0) << 12);
				emu->x89.channel[channel_number].r[X89_R_TP].tag = (data[2] >> 3) & 1;
				emu->x89.channel[channel_number].psw = data[3];
				x89_channel_busy_set(emu, channel_number);
				continue;
			case 6:
				// halt channel and save tp/psw
				x89_channel_busy_clear(emu, channel_number);
				data[0] = emu->x89.channel[channel_number].r[X89_R_TP].address;
				data[1] = emu->x89.channel[channel_number].r[X89_R_TP].address >> 8;
				data[2] = ((emu->x89.channel[channel_number].r[X89_R_TP].address >> 12) & 0xF0) | (emu->x89.channel[channel_number].r[X89_R_TP].tag << 3);
				data[3] = emu->x89.channel[channel_number].psw;
				x86_memory_write_external(emu, emu->x89.channel[channel_number].pp, 4, data);
				continue;
			case 7:
				// halt channel and do not save tp/psw
				x89_channel_busy_clear(emu, channel_number);
				continue;
			}
			// interrupt control
			switch((ccw >> 3) & 3)
			{
			case 0:
				break;
			case 1:
				emu->x89.channel[channel_number].psw &= ~X89_PSW_IS;
				break;
			case 2:
				emu->x89.channel[channel_number].psw |= X89_PSW_IC;
				break;
			case 3:
				emu->x89.channel[channel_number].psw &= ~X89_PSW_IS;
				emu->x89.channel[channel_number].psw &= ~X89_PSW_IC;
				break;
			}
			// bus load limit
			if((ccw & 0x20) != 0)
			{
				emu->x89.channel[channel_number].psw |= X89_PSW_B;
			}
			else
			{
				emu->x89.channel[channel_number].psw &= ~X89_PSW_B;
			}
			// channel priority
			if((ccw & 0x80) != 0)
			{
				emu->x89.channel[channel_number].psw |= X89_PSW_P;
			}
			else
			{
				emu->x89.channel[channel_number].psw &= ~X89_PSW_P;
			}
		}
	}
}

static inline void x89_read(x86_state_t * emu, x89_address_t addr, uaddr_t count, void * buffer)
{
	if(addr.tag == 0)
	{
		x86_memory_read_external(emu, addr.address, count, buffer);
	}
	else
	{
		// TODO: depends on configuration
		x86_input(emu, (uint16_t)addr.address, count, buffer);
	}
}

static inline uint8_t x89_read8(x86_state_t * emu, x89_address_t addr)
{
	uint8_t value;
	x89_read(emu, addr, 1, &value);
	return value;
}

static inline uint16_t x89_read16(x86_state_t * emu, x89_address_t addr)
{
	uint16_t value;
	x89_read(emu, addr, 2, &value);
	return le16toh(value);
}

static inline uint32_t x89_read32(x86_state_t * emu, x89_address_t addr)
{
	uint32_t value;
	x89_read(emu, addr, 4, &value);
	return le32toh(value);
}

static inline void x89_read_pair16(x86_state_t * emu, x89_address_t addr, uint16_t * first, uint16_t * second)
{
	uint16_t pair[2];
	x89_read(emu, addr, 4, &pair);
	*first = le16toh(pair[0]);
	*second = le16toh(pair[1]);
}

static inline x89_address_t x89_read_address(x86_state_t * emu, x89_address_t addr)
{
	x89_address_t result;
	uint8_t data[3];
	x89_read(emu, addr, 3, data);
	result.address = data[0] | (data[1] << 8) | ((data[2] & 0xF0) << 12);
	result.tag = (data[2] >> 3) & 1;
	return result;
}

static inline void x89_write(x86_state_t * emu, x89_address_t addr, uaddr_t count, const void * buffer)
{
	// TODO: depends on configuration
	if(addr.tag == 0)
	{
		x86_memory_write_external(emu, addr.address, count, buffer);
	}
	else
	{
		x86_output(emu, (uint16_t)addr.address, count, buffer);
	}
}

static inline void x89_write8(x86_state_t * emu, x89_address_t addr, uint8_t value)
{
	x89_write(emu, addr, 1, &value);
}

static inline void x89_write16(x86_state_t * emu, x89_address_t addr, uint16_t value)
{
	value = htole16(value);
	x89_write(emu, addr, 2, &value);
}

static inline void x89_write32(x86_state_t * emu, x89_address_t addr, uint32_t value)
{
	value = htole32(value);
	x89_write(emu, addr, 4, &value);
}

static inline void x89_write_address(x86_state_t * emu, x89_address_t addr, x89_address_t value)
{
	uint8_t data[3];
	data[0] = value.address;
	data[1] = value.address >> 8;
	data[2] = ((value.address >> 12) & 0xF0) | (value.tag << 3);
	x89_write(emu, addr, 3, data);
}

struct x89_parser_state
{
	x89_parser_t prs[1];
	x86_state_t * emu;
	unsigned channel_number;
};

static inline uint8_t x89_fetch8(x89_parser_t * prs, x86_state_t * emu, unsigned channel_number)
{
	if(emu != NULL)
	{
		uint16_t value = x89_read8(emu, emu->x89.channel[channel_number].r[X89_R_TP]);
		emu->x89.channel[channel_number].r[X89_R_TP].address += 1;
		if(prs != NULL)
			prs->current_position += 1;
		return value;
	}
	else
	{
		return prs->fetch8(prs);
	}
}

static inline uint16_t x89_fetch16(x89_parser_t * prs, x86_state_t * emu, unsigned channel_number)
{
	if(emu != NULL)
	{
		uint16_t value = x89_read16(emu, emu->x89.channel[channel_number].r[X89_R_TP]);
		emu->x89.channel[channel_number].r[X89_R_TP].address += 2;
		if(prs != NULL)
			prs->current_position += 2;
		return value;
	}
	else
	{
		return prs->fetch16(prs);
	}
}

/*static inline uint32_t x89_fetch32(x86_state_t * emu, unsigned channel_number)
{
	uint16_t value = x89_read32(emu, emu->x89.channel[channel_number].r[X89_R_TP]);
	emu->x89.channel[channel_number].r[X89_R_TP].address += 4;
	return value;
}*/

static uint8_t _x89_fetch8(x89_parser_t * prs)
{
	struct x89_parser_state * str = (struct x89_parser_state *)prs;
	return x89_fetch8(prs, str->emu, str->channel_number);
}

static uint16_t _x89_fetch16(x89_parser_t * prs)
{
	struct x89_parser_state * str = (struct x89_parser_state *)prs;
	return x89_fetch16(prs, str->emu, str->channel_number);
}

static const char * x89_register_name[] =
{
	"ga", "gb", "gc", "bc", "tp", "ix", "cc", "mc"
};

static const char * x89_base_register_name[] =
{
	"ga", "gb", "gc", "pp"
};

#define _OPSIZE (ins & 1)
#define _IMSIZE ((ins >> 3) & 3)
#define _REGFLD ((ins >> 5) & 7)
#define _BASEFLD ((ins >> 8) & 3)

static inline bool x89_channel_transfer(x86_state_t * emu, unsigned channel_number)
{
	if((emu->x89.channel[channel_number].psw & X89_PSW_XF) == 0)
		return false;

	// TODO: verify
	// TODO: also check fields TX, C, L
	x89_regnum_t gs = (emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_S) == 0 ? X89_R_GA : X89_R_GB;
	x89_regnum_t gd = 1 - gs;

	size_t dst_size = (emu->x89.channel[channel_number].psw & X89_PSW_D) == 0 ? 1 : 2;
	size_t src_size = (emu->x89.channel[channel_number].psw & X89_PSW_S) == 0 ? 1 : 2;

	if((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_TBC_MASK) != 0)
	{
		if((emu->x89.channel[channel_number].r[X89_R_BC].address & 0xFFFF) == 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
			switch((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_TBC_MASK) >> X89_CC_TBC_SHIFT)
			{
			case 2:
				emu->x89.channel[channel_number].r[X89_R_TP].address += 4;
				break;
			case 3:
				emu->x89.channel[channel_number].r[X89_R_TP].address += 8;
				break;
			}
			return false;
		}
	}

	// TODO: do we count source or destination?
	emu->x89.channel[channel_number].r[X89_R_BC].address = (emu->x89.channel[channel_number].r[X89_R_BC].address - 1) & 0xFFFF;

	uint16_t data;
	if(src_size == 1)
		data = x89_read8(emu, emu->x89.channel[channel_number].r[gs]);
	else
		data = x89_read16(emu, emu->x89.channel[channel_number].r[gs]);

	if((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_F0) != 0)
	{
		emu->x89.channel[channel_number].r[gs].address += src_size;
	}

	if((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_TS) != 0)
	{
		emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
	}

	uint16_t mc = emu->x89.channel[channel_number].r[X89_R_MC].address;
	switch((emu->x89.channel[channel_number].r[X89_R_CC].address >> X89_CC_TSH_SHIFT) & 7)
	{
	case 1:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) == 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
		}
		break;
	case 2:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) == 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
			emu->x89.channel[channel_number].r[X89_R_TP].address += 4;
		}
		break;
	case 3:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) == 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
			emu->x89.channel[channel_number].r[X89_R_TP].address += 8;
		}
		break;
	case 4:
		break;
	case 5:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) != 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
		}
		break;
	case 6:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) != 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
			emu->x89.channel[channel_number].r[X89_R_TP].address += 4;
		}
		break;
	case 7:
		// TODO: what is considered a match?
		if(((data ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) != 0)
		{
			emu->x89.channel[channel_number].psw &= ~X89_PSW_XF;
			emu->x89.channel[channel_number].r[X89_R_TP].address += 8;
		}
		break;
	}

	if((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_TR) != 0)
	{
		x89_address_t addr = emu->x89.channel[channel_number].r[X89_R_GC];
		addr.address += data & 0xFF;
		data = x89_read8(emu, addr);
	}

	if(dst_size == 1)
		x89_write8(emu, emu->x89.channel[channel_number].r[gd], data);
	else
		x89_write16(emu, emu->x89.channel[channel_number].r[gd], data);
	if((emu->x89.channel[channel_number].r[X89_R_CC].address & X89_CC_F1) != 0)
	{
		emu->x89.channel[channel_number].r[gd].address += dst_size;
	}

	return true;
}

static inline void x89_parse(x89_parser_t * prs, x86_state_t * emu, unsigned channel_number, bool disassemble, bool execute);

static inline void x89_channel_step(x86_state_t * emu, unsigned channel_number, bool disassemble, bool execute)
{
	if(x89_channel_transfer(emu, channel_number))
		return;

	x89_parser_t prs[1];
	prs->current_position = emu->x89.channel[channel_number].r[X89_R_TP].address;
	prs->fetch8 = _x89_fetch8;
	prs->fetch16 = _x89_fetch16;

	x89_parse(prs, emu, channel_number, disassemble, execute);

	if(emu->x89.channel[channel_number].start_transfer)
	{
		emu->x89.channel[channel_number].start_transfer = false;
		emu->x89.channel[channel_number].psw |= X89_PSW_XF;
	}
}

x89_parser_t * x89_setup_parser(x86_state_t * emu, unsigned channel_number)
{
	struct x89_parser_state * str = malloc(sizeof(struct x89_parser_state));
	str->prs->current_position = emu->x89.channel[channel_number].r[X89_R_TP].address;
	str->prs->fetch8 = _x89_fetch8;
	str->prs->fetch16 = _x89_fetch16;
	str->emu = emu;
	str->channel_number = channel_number;
	return (x89_parser_t *)str;
}


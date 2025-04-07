
// Handles 8080 emulation mode as well as emulating a full 8080/Z80

static inline uint8_t x86_get_low(uint16_t value)
{
	return value;
}

static inline uint8_t x86_get_high(uint16_t value)
{
	return value >> 8;
}

static inline void x86_set_low(uint16_t * reference, uint8_t value)
{
	*reference = (*reference & ~0xFF) | value;
}

static inline void x86_set_high(uint16_t * reference, uint8_t value)
{
	*reference = (*reference & ~0xFF00) | (value << 8);
}

static inline uint8_t x80_register_get8(x80_state_t * emu, x80_regnum_t prefix, int number, int8_t offset)
{
	switch(number)
	{
	case 0:
		return x86_get_high(emu->bank[emu->main_bank].bc);
	case 1:
		return x86_get_low(emu->bank[emu->main_bank].bc);
	case 2:
		return x86_get_high(emu->bank[emu->main_bank].de);
	case 3:
		return x86_get_low(emu->bank[emu->main_bank].de);
	case 4:
		switch(prefix)
		{
		case X80_R_IX:
			return x86_get_high(emu->ix);
		case X80_R_IY:
			return x86_get_high(emu->iy);
		default:
			return x86_get_high(emu->bank[emu->main_bank].hl);
		}
	case 5:
		switch(prefix)
		{
		case X80_R_IX:
			return x86_get_low(emu->ix);
		case X80_R_IY:
			return x86_get_low(emu->iy);
		default:
			return x86_get_low(emu->bank[emu->main_bank].hl);
		}
	case 6:
		switch(prefix)
		{
		case X80_R_IX:
			return x80_memory_read8(emu, emu->ix + offset);
		case X80_R_IY:
			return x80_memory_read8(emu, emu->iy + offset);
		default:
			return x80_memory_read8(emu, emu->bank[emu->main_bank].hl);
		}
	case 7:
		return x86_get_high(emu->bank[emu->main_bank].af);
	default:
		assert(false);
	}
}

static inline void x80_register_set8(x80_state_t * emu, int prefix, int number, int8_t offset, uint8_t value)
{
	switch(number)
	{
	case 0:
		x86_set_high(&emu->bank[emu->main_bank].bc, value);
		break;
	case 1:
		x86_set_low(&emu->bank[emu->main_bank].bc, value);
		break;
	case 2:
		x86_set_high(&emu->bank[emu->main_bank].de, value);
		break;
	case 3:
		x86_set_low(&emu->bank[emu->main_bank].de, value);
		break;
	case 4:
		switch(prefix)
		{
		case X80_R_IX:
			x86_set_high(&emu->ix, value);
			break;
		case X80_R_IY:
			x86_set_high(&emu->iy, value);
			break;
		default:
			x86_set_high(&emu->bank[emu->main_bank].hl, value);
			break;
		}
		break;
	case 5:
		switch(prefix)
		{
		case X80_R_IX:
			x86_set_low(&emu->ix, value);
			break;
		case X80_R_IY:
			x86_set_low(&emu->iy, value);
			break;
		default:
			x86_set_low(&emu->bank[emu->main_bank].hl, value);
			break;
		}
		break;
	case 6:
		switch(prefix)
		{
		case X80_R_IX:
			x80_memory_write8(emu, emu->ix + offset, value);
			break;
		case X80_R_IY:
			x80_memory_write8(emu, emu->iy + offset, value);
			break;
		default:
			x80_memory_write8(emu, emu->bank[emu->main_bank].hl, value);
			break;
		}
		break;
	case 7:
		x86_set_high(&emu->bank[emu->main_bank].af, value);
		break;
	default:
		assert(false);
	}
}

static inline uint8_t x80_fetch8(x80_parser_t * prs, x80_state_t * emu)
{
	if(emu != NULL)
	{
		return x80_memory_fetch8(emu);
	}
	else
	{
		return prs->fetch8(prs);
	}
}

static inline uint16_t x80_fetch16(x80_parser_t * prs, x80_state_t * emu)
{
	if(emu != NULL)
	{
		return x80_memory_fetch16(emu);
	}
	else
	{
		return prs->fetch16(prs);
	}
}

static inline void x80_push16(x80_state_t * emu, uint16_t value)
{
	uint16_t sp = emu->sp;
	sp -= 2;
	emu->sp = sp;
	x80_memory_write16(emu, sp, value);
}

static inline uint16_t x80_pop16(x80_state_t * emu)
{
	uint16_t sp = emu->sp;
	emu->sp += 2;
	return x80_memory_read16(emu, sp);
}


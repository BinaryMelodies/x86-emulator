
//// Floating point and x87 support

static inline void x87_convert_from_float80(float80_t value, uint64_t * fraction, uint16_t * exponent, bool * sign)
{
	int exp;
	*fraction = (uint64_t)ldexpl(frexpl(value, &exp), 64);
	*sign = signbit(value);
	*exponent = exp + 0x3FFE;
}

static inline float80_t x87_convert_to_float80(uint64_t fraction, uint16_t exponent, bool sign)
{
	return ldexpl(copysign((long double)fraction, sign ? -1.0 : 1.0), (int)exponent - (0x3FFE + 64));
}

static inline float80_t x87_convert32_to_float(uint32_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	// TODO: this is only for normals
	fraction = ((uint64_t)(value & 0x007FFFFF) << 40) | 0x8000000000000000;
	exponent = ((value & 0x7F800000) >> 23) + 127 - 16383;
	sign = (value & 0x80000000) != 0;
	return x87_convert_to_float80(fraction, exponent, sign);
}

static inline float80_t x87_convert64_to_float(uint64_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	// TODO: this is only for normals
	fraction = ((value & 0x000FFFFFFFFFFFFF) << 11) | 0x8000000000000000;
	exponent = ((value & 0x7FF0000000000000) >> 52) + 1023 - 16383;
	sign = (value & 0x8000000000000000) != 0;
	return x87_convert_to_float80(fraction, exponent, sign);
}

static inline uint32_t x87_convert32_from_float(float80_t value)
{
	uint32_t result;
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	x87_convert_from_float80(value, &fraction, &exponent, &sign);
	// TODO: this is only for normals, in range
	result = (fraction & 0x7FFFFFFFFFFFFFFF) >> 40;
	result |= (uint32_t)((exponent + 16383 - 127) & 0xFF) << 23;
	if(sign)
		result |= 0x80000000;
	return result;
}

static inline uint64_t x87_convert64_from_float(float80_t value)
{
	uint64_t result;
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	x87_convert_from_float80(value, &fraction, &exponent, &sign);
	// TODO: this is only for normals, in range
	result = (fraction & 0x7FFFFFFFFFFFFFFF) >> 11;
	result |= (uint64_t)((exponent + 16383 - 1023) & 0x7FF) << 52;
	if(sign)
		result |= 0x8000000000000000;
	return result;
}

static inline bool x87_is_busy(x86_state_t * emu)
{
	return (emu->x87.sw & X87_SW_B) != 0;
}

static inline void x87_trigger_interrupt(x86_state_t * emu)
{
	if(emu->x87.fpu_type == X87_FPU_8087)
	{
		if((emu->x87.cw & X87_CW_IEM) == 0)
		{
			emu->emulation_result = X86_RESULT(X86_RESULT_IRQ, emu->x87.irq_number);
			longjmp(emu->exc[emu->fetch_mode], 1);
		}
	}
	else
	{
		if(emu->x87.fpu_type == X87_FPU_INTEGRATED && (emu->cr[0] & X86_CR0_NE) == 0)
		{
			emu->emulation_result = X86_RESULT(X86_RESULT_IRQ, emu->x87.irq_number);
			longjmp(emu->exc[emu->fetch_mode], 1);
		}
		else
		{
			x86_trigger_interrupt(emu, X86_EXC_MF | X86_EXC_FAULT, 0);
		}
	}
}

static inline void x87_signal_interrupt(x86_state_t * emu, int intnum)
{
	emu->x87.sw |= intnum;

	if(emu->x87.fpu_type >= X87_FPU_287)
	{
		emu->x87.cw |= X87_SW_ES;
		if(emu->x87.fpu_type == X87_FPU_INTEGRATED)
			emu->x87.cw |= X87_SW_B;
	}

	if((emu->x87.cw & intnum) == 0)
	{
		if(emu->x87.fpu_type == X87_FPU_8087)
		{
			emu->x87.cw |= X87_SW_IR; // same position as the X87_SW_ES flag
			x87_trigger_interrupt(emu);
		}
	}
}

static inline float32_t x87_float80_to_float32(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline float80_t x87_float32_to_float80(x86_state_t * emu, float32_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline float64_t x87_float80_to_float64(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline float80_t x87_float64_to_float80(x86_state_t * emu, float64_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline int64_t x87_float80_to_int64(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline float80_t x87_int64_to_float80(x86_state_t * emu, int64_t value)
{
	(void) emu;
	// TODO: without float80 support
	return value;
}

static inline float80_t x87_packed80_to_float80(const uint8_t bytes[10])
{
	int64_t int_value = 0;
	for(int digit_pair = 0, tens = 1; digit_pair < 9; digit_pair++, tens *= 100)
	{
		int_value +=
			tens * (bytes[digit_pair] & 0xF)
			+ tens * 10 * ((bytes[digit_pair] >> 4) & 0xF);
	}
	if((bytes[0] & 0x80) == 0)
		return (float80_t)int_value;
	else
		return -(float80_t)int_value;
}

static inline void x87_float80_to_packed80(float80_t value, uint8_t bytes[10])
{
	int64_t int_value = (int64_t)(value + 0.5);
	bytes[0] = value < 0 ? 0x80 : 0;
	if(int_value < 0)
		int_value = -int_value;
	for(int digit_pair = 0; digit_pair < 9; digit_pair++)
	{
		bytes[digit_pair] = (int_value % 10) + (((int_value / 10) % 10) << 4);
	}
}

static inline unsigned x87_get_sw_top(x86_state_t * emu)
{
	return (emu->x87.sw >> X87_SW_TOP_SHIFT) & 7;
}

static inline void x87_set_sw_top(x86_state_t * emu, unsigned top)
{
	emu->x87.sw = (emu->x87.sw & ~(7 << X87_SW_TOP_SHIFT)) | ((top & 7) << X87_SW_TOP_SHIFT);
}

static inline unsigned x87_register_number(x86_state_t * emu, x86_regnum_t number)
{
	return (x87_get_sw_top(emu) + number) & 7;
}

static inline int x87_tag_get(x86_state_t * emu, int number)
{
	number &= 7;
	return (emu->x87.tw >> (number * 2)) & 3;
}

static inline void x87_tag_set(x86_state_t * emu, int number, int tag)
{
	number &= 7;
	emu->x87.tw = (emu->x87.tw & ~(3 << (number * 2))) | ((tag & 3) << (number * 2));
}

static inline float80_t x87_restrict_precision(x86_state_t * emu, float80_t value)
{
	switch((emu->x87.cw >> X87_CW_PC_SHIFT) & 3)
	{
	case 0:
		// TODO: 24-bit
		return value;
	case 2:
		// TODO: 53-bit
		return value;
	case 1:
		// reserved
	case 3:
		return value;
	default:
		assert(false);
	}
}

static inline float80_t x87_register_get80(x86_state_t * emu, x86_regnum_t number)
{
	/* TODO: other fields? (tag) */
	number = x87_register_number(emu, number);
	if(x87_tag_get(emu, number) == X87_TAG_EMPTY)
		x87_signal_interrupt(emu, X87_CW_IM);

#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		return emu->x87.bank[emu->x87.current_bank].fpr[number].f;
	}
	else
	{
		uint16_t exponent = emu->x87.bank[emu->x87.current_bank].fpr[number].exponent;
		return x87_convert_to_float80(emu->x87.bank[emu->x87.current_bank].fpr[number].mmx.q[0], exponent & 0x7FFF, (exponent & 0x8000) != 0);
	}
#else
	return emu->x87.bank[emu->x87.current_bank].fpr[number].f;
#endif
}

static inline float80_t x87_register_get80_bank(x86_state_t * emu, x86_regnum_t number, unsigned bank_number)
{
	/* TODO: other fields? (tag) */
	number = x87_register_number(emu, number);
	if(x87_tag_get(emu, number) == X87_TAG_EMPTY)
		x87_signal_interrupt(emu, X87_CW_IM);

#if _SUPPORT_FLOAT80
	if(emu->x87.bank[bank_number].fpr[number].isfp)
	{
		return emu->x87.bank[bank_number].fpr[number].f;
	}
	else
	{
		uint16_t exponent = emu->x87.bank[bank_number].fpr[number].exponent;
		return x87_convert_to_float80(emu->x87.bank[bank_number].fpr[number].mmx.q[0], exponent & 0x7FFF, (exponent & 0x8000) != 0);
	}
#else
	return emu->x87.bank[bank_number].fpr[number].f;
#endif
}

static inline void x87_register_tag(x86_state_t * emu, x86_regnum_t number)
{
	number = x87_register_number(emu, number);
	// TODO: emu->x87.bank[emu->x87.current_bank].fpr[number].isfp = true;
	switch(fpclassify(emu->x87.bank[emu->x87.current_bank].fpr[number].f))
	{
	case FP_ZERO:
		x87_tag_set(emu, number, X87_TAG_ZERO);
		break;
	case FP_NORMAL:
		x87_tag_set(emu, number, X87_TAG_VALID);
		break;
	case FP_NAN:
	case FP_INFINITE:
	case FP_SUBNORMAL:
		x87_tag_set(emu, number, X87_TAG_SPECIAL);
		break;
	}
}

static inline void x87_register_set80(x86_state_t * emu, x86_regnum_t number, float80_t value)
{
	// TODO: on stack underflow, issue #IS
	value = x87_restrict_precision(emu, value);
	number = x87_register_number(emu, number);
#if _SUPPORT_FLOAT80
	emu->x87.bank[emu->x87.current_bank].fpr[number].isfp = true;
#endif
	emu->x87.bank[emu->x87.current_bank].fpr[number].f = value;
	switch(fpclassify(value))
	{
	case FP_ZERO:
		x87_tag_set(emu, number, X87_TAG_ZERO);
		break;
	case FP_NORMAL:
		x87_tag_set(emu, number, X87_TAG_VALID);
		break;
	case FP_NAN:
	case FP_INFINITE:
	case FP_SUBNORMAL:
		x87_tag_set(emu, number, X87_TAG_SPECIAL);
		break;
	}
}

static inline void x87_register_set80_bank(x86_state_t * emu, x86_regnum_t number, int bank_number, float80_t value)
{
	// TODO: on stack underflow, issue #IS
	value = x87_restrict_precision(emu, value);
	number = x87_register_number(emu, number);
#if _SUPPORT_FLOAT80
	emu->x87.bank[bank_number].fpr[number].isfp = true;
#endif
	emu->x87.bank[bank_number].fpr[number].f = value;
	switch(fpclassify(value))
	{
	case FP_ZERO:
		x87_tag_set(emu, number, X87_TAG_ZERO);
		break;
	case FP_NORMAL:
		x87_tag_set(emu, number, X87_TAG_VALID);
		break;
	case FP_NAN:
	case FP_INFINITE:
	case FP_SUBNORMAL:
		x87_tag_set(emu, number, X87_TAG_SPECIAL);
		break;
	}
}

static inline void x87_register_free(x86_state_t * emu, x86_regnum_t number)
{
	number = x87_register_number(emu, number);
	x87_tag_set(emu, number, X87_TAG_EMPTY);
}

static inline float80_t x87_pop(x86_state_t * emu)
{
	if(x87_tag_get(emu, 0) == X87_TAG_EMPTY)
		x87_signal_interrupt(emu, X87_CW_IM);

	float80_t value = x87_register_get80(emu, 0);
	x87_set_sw_top(emu, x87_get_sw_top(emu) + 1);
	return value;
}

static inline void x87_push(x86_state_t * emu, float80_t value)
{
	if(x87_tag_get(emu, 7) != X87_TAG_EMPTY)
		x87_signal_interrupt(emu, X87_CW_IM);

	x87_set_sw_top(emu, x87_get_sw_top(emu) - 1);
	x87_register_set80(emu, 0, value);
}

static inline uint64_t x86_mmx_get(x86_state_t * emu, x86_regnum_t number)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.bank[emu->x87.current_bank].fpr[number].isfp)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		x87_convert_from_float80(emu->x87.bank[emu->x87.current_bank].fpr[number].f, &fraction, &exponent, &sign);
		return fraction;
	}
	else
	{
		return emu->x87.bank[emu->x87.current_bank].fpr[number].mmx.q[0];
	}
#else
	return emu->x87.bank[emu->x87.current_bank].fpr[number].mmx.q[0];
#endif
}

static inline void x86_mmx_set(x86_state_t * emu, x86_regnum_t number, uint64_t value)
{
	emu->x87.bank[emu->x87.current_bank].fpr[number].exponent = 0xFFFF;
	emu->x87.bank[emu->x87.current_bank].fpr[number].mmx.q[0] = value;
}

static inline float80_t x87_f2xm1(x86_state_t * emu, float80_t value)
{
	(void) emu;
	(void) value;
	return expm1l(log2l(value)); // TODO
}

static inline float80_t x87_fabs(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return fabsl(value);
}

static inline float80_t x87_fadd(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value1 + value2;
}

static inline float80_t x87_fchs(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return -value;
}

static inline void x87_fcom(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	if(value1 > value2)
	{
		emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
	}
	else if(value1 > value2)
	{
		emu->x87.sw &= ~X87_SW_C3;
		emu->x87.sw |= X87_SW_C0;
	}
	else if(value1 == value2)
	{
		emu->x87.sw |= X87_SW_C3;
		emu->x87.sw &= ~X87_SW_C0;
	}
	else
	{
		emu->x87.sw |= X87_SW_C3;
		emu->x87.sw |= X87_SW_C0;
	}
}

static inline float80_t x87_fdiv(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value1 / value2;
}

static inline float80_t x87_fmul(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value1 * value2;
}

static inline float80_t x87_fprem(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	(void) value1;
	(void) value2;
	// TODO: non long double versions
	// TODO: precision/rounding
	return 0; // TODO
}

static inline void x87_fptan(x86_state_t * emu, float80_t value, float80_t * result1, float80_t * result2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	*result1 = 1.0;
	*result2 = tanl(value);
}

static inline float80_t x87_fpatan(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return atan2l(value1, value2);
}

static inline float80_t x87_frndint(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return x87_int64_to_float80(emu, x87_float80_to_int64(emu, value));
}

static inline float80_t x87_fscale(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return ldexpl(value1, x87_float80_to_int64(emu, value2));
}

static inline float80_t x87_fsub(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value1 - value2;
}

static inline float80_t x87_fsqrt(x86_state_t * emu, float80_t value)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return sqrtl(value);
}

static inline void x87_fxam(x86_state_t * emu, float80_t value, bool is_empty)
{
	if(is_empty)
	{
		emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
	}
	else
	{
		// TODO: emulate unnormals?
		switch(fpclassify(value))
		{
		case FP_NAN:
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C2);
			emu->x87.sw |= X87_SW_C0;
			break;
		//case FP_UNNORMAL:
		//	emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C2 | X87_SW_C0);
		//	break;
		case FP_INFINITE:
			emu->x87.sw &= ~(X87_SW_C3);
			emu->x87.sw |= X87_SW_C2 | X87_SW_C0;
			break;
		case FP_ZERO:
			emu->x87.sw &= ~(X87_SW_C2 | X87_SW_C0);
			emu->x87.sw |= X87_SW_C3;
			break;
		case FP_SUBNORMAL:
		case FP_NORMAL:
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
			emu->x87.sw |= X87_SW_C2;
			break;
		}
		if(signbit(value))
			emu->x87.sw |= X87_SW_C1;
		else
			emu->x87.sw &= ~X87_SW_C1;
	}
}

static inline void x87_fxtract(x86_state_t * emu, float80_t value, float80_t * result1, float80_t * result2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	int exp;
	*result1 = frexpl(value, &exp);
	*result2 = x87_int64_to_float80(emu, exp);
}

static inline float80_t x87_fyl2x(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value2 * log2l(value1);
}

static inline float80_t x87_fyl2xp1(x86_state_t * emu, float80_t value1, float80_t value2)
{
	(void) emu;
	// TODO: non long double versions
	// TODO: precision/rounding
	return value2 * log2l(value1 + 1);
}

static inline void x87_state_save_registers(x86_state_t * emu, x86_segnum_t segment, uoff_t x86_offset, uoff_t offset)
{
	for(int i = 0; i < 8; i++)
	{
		float80_t fpr = x87_register_get80(emu, i);
		x87_memory_segmented_write80fp(emu, segment, x86_offset, offset + 10 * i, fpr);
	}
}

static inline void x87_state_restore_registers(x86_state_t * emu, x86_segnum_t segment, uoff_t x86_offset, uoff_t offset)
{
	for(int i = 0; i < 8; i++)
	{
		float80_t fpr = x87_memory_segmented_read80fp(emu, segment, x86_offset, offset + 10 * i);

		int number = x87_register_number(emu, i);
#if _SUPPORT_FLOAT80
		emu->x87.bank[emu->x87.current_bank].fpr[number].isfp = true;
#endif
		emu->x87.bank[emu->x87.current_bank].fpr[number].f = fpr;
		if(emu->x87.fpu_type >= X87_FPU_387)
		{
			// TODO: also for environment_restore
			if(x87_tag_get(emu, i) != X87_TAG_EMPTY)
				x87_register_tag(emu, i);
		}
	}
}

static inline void x87_environment_save_real_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	// testing on real hardware shows that this is the behavior, even in unreal mode
	uint32_t fip = emu->x87.fip + (emu->x87.fcs << 4);
	uint32_t fdp = emu->x87.fdp + (emu->x87.fds << 4);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0000, emu->x87.cw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0002, emu->x87.sw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0004, emu->x87.tw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0006, fip);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0008, ((fip >> 4) & 0xF000) | (emu->x87.fop & 0x07FF));
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x000A, fdp);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x000C, (fdp >> 4) & 0xF000);
}

static inline void x87_environment_restore_real_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	emu->x87.cw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0000);
	emu->x87.sw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0002);
	emu->x87.tw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0004);
	emu->x87.fip = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0006);
	uint16_t fip_fop = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0008);
	emu->x87.fip |= (fip_fop & 0xF000) << 4;
	emu->x87.fop = 0xD800 | (fip_fop & 0x07FF);
	emu->x87.fcs = 0;
	emu->x87.fdp = x87_memory_segmented_read16(emu, segment, offset, offset + 0x000A);
	emu->x87.fdp |= (x87_memory_segmented_read16(emu, segment, offset, offset + 0x000C) & 0xF000) << 4;
	emu->x87.fds = 0;
}

static inline void x87_state_save_real_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_save_real_mode16(emu, segment, offset);
	x87_state_save_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_state_restore_real_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_real_mode16(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_environment_save_real_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	// testing on real hardware shows that this is the behavior, even in unreal mode
	uint32_t fip = emu->x87.fip + (emu->x87.fcs << 4);
	uint32_t fdp = emu->x87.fdp + (emu->x87.fds << 4);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0000, emu->x87.cw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0004, emu->x87.sw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0008, emu->x87.tw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x000C, fip & 0xFFFF);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0010, ((fip >> 4) & ~0xFFF) | (emu->x87.fop & 0x07FF));
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0014, fdp & 0xFFFF);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0018, (fdp >> 4) & ~0x0FFF);
}

static inline void x87_environment_restore_real_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	emu->x87.cw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0000);
	emu->x87.sw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0004);
	emu->x87.tw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0008);
	emu->x87.fip = x87_memory_segmented_read32(emu, segment, offset, offset + 0x000C) & 0xFFFF;
	uint32_t fip_fop = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0010);
	emu->x87.fip |= (fip_fop & 0x0FFFF000) << 4;
	emu->x87.fop = 0xD800 | (fip_fop & 0x07FF);
	emu->x87.fdp = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0014);
	emu->x87.fdp |= (x87_memory_segmented_read16(emu, segment, offset, offset + 0x000C) & 0x0FFFF000) << 4;
	emu->x87.fds = 0;
}

static inline void x87_state_save_real_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_save_real_mode32(emu, segment, offset);
	x87_state_save_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_state_restore_real_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_real_mode32(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_environment_save_protected_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0000, emu->x87.cw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0002, emu->x87.sw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0004, emu->x87.tw);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0006, emu->x87.fip);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x0008, emu->x87.fcs);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x000A, emu->x87.fdp);
	x87_memory_segmented_write16(emu, segment, offset, offset + 0x000C, emu->x87.fds);
}

static inline void x87_environment_restore_protected_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	emu->x87.cw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0000);
	emu->x87.sw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0002);
	emu->x87.tw = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0004);
	emu->x87.fip = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0006);
	emu->x87.fcs = x87_memory_segmented_read16(emu, segment, offset, offset + 0x0008);
	emu->x87.fdp = x87_memory_segmented_read16(emu, segment, offset, offset + 0x000A);
	emu->x87.fds = x87_memory_segmented_read16(emu, segment, offset, offset + 0x000C);
}

static inline void x87_state_save_protected_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_save_protected_mode16(emu, segment, offset);
	x87_state_save_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_state_restore_protected_mode16(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_protected_mode16(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_environment_save_protected_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0000, emu->x87.cw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0004, emu->x87.sw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0008, emu->x87.tw);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x000C, emu->x87.fip);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0010, emu->x87.fcs | ((uint32_t)(emu->x87.fop & 0x07FF) << 16));
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0014, emu->x87.fdp);
	x87_memory_segmented_write32(emu, segment, offset, offset + 0x0018, emu->x87.fds);
}

static inline void x87_environment_restore_protected_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	emu->x87.cw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0000);
	emu->x87.sw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0004);
	emu->x87.tw = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0008);
	emu->x87.fip = x87_memory_segmented_read32(emu, segment, offset, offset + 0x000C);
	uint32_t fcs_fop = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0010);
	emu->x87.fcs = fcs_fop;
	emu->x87.fop = 0xD800 | ((fcs_fop >> 16) & 0x07FF);
	emu->x87.fdp = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0014);
	emu->x87.fds = x87_memory_segmented_read32(emu, segment, offset, offset + 0x0018);
}

static inline void x87_state_save_protected_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_save_protected_mode32(emu, segment, offset);
	x87_state_save_registers(emu, segment, offset, offset + 0x000E);
}

static inline void x87_state_restore_protected_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_protected_mode32(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x000E);
}

static inline uint8_t x87_abridge_tw(uint16_t tw)
{
	uint8_t abridged_tw = 0;
	for(int i = 0; i < 8; i++)
	{
		if(((tw >> (i * 2)) & 3) != 3)
			abridged_tw = 1 << i;
	}
	return abridged_tw;
}

static inline void x87_state_save_extended_common(x86_state_t * emu, x86_segnum_t segment, uoff_t offset, int xmm_count)
{
	x86_memory_segmented_write16(emu, segment, offset + 0x0000, emu->x87.cw);
	x86_memory_segmented_write16(emu, segment, offset + 0x0002, emu->x87.sw);
	x86_memory_segmented_write8(emu, segment, offset + 0x0004, x87_abridge_tw(emu->x87.tw));
	x86_memory_segmented_write16(emu, segment, offset + 0x0006, emu->x87.fop & 0x07FF);
	x86_memory_segmented_write32(emu, segment, offset + 0x0018, emu->mxcsr);
	x86_memory_segmented_write32(emu, segment, offset + 0x0018, 0); // TODO: MXCSR_MASK
	for(int i = 0; i < 8; i++)
	{
		float80_t fpr = x87_register_get80(emu, i);
		x86_memory_segmented_write80fp(emu, segment, offset + 0x0020 + 16 * i, fpr);
	}
	for(int i = 0; i < xmm_count; i++)
	{
		x86_memory_segmented_write128(emu, segment, offset + 0x00A0 + 16 * i, &emu->xmm[i]);
	}
}

static inline void x87_state_restore_extended_common(x86_state_t * emu, x86_segnum_t segment, uoff_t offset, int xmm_count)
{
	emu->x87.cw = x86_memory_segmented_read16(emu, segment, offset + 0x0000);
	emu->x87.sw = x86_memory_segmented_read16(emu, segment, offset + 0x0002);
	uint8_t abridged_tw = x86_memory_segmented_read8(emu, segment, offset + 0x0004);
	emu->x87.fop = 0xD800 | x86_memory_segmented_read16(emu, segment, offset + 0x0006);
	emu->mxcsr = x86_memory_segmented_read32(emu, segment, offset + 0x0018);
	for(int i = 0; i < 8; i++)
	{
		float80_t fpr = x86_memory_segmented_read80fp(emu, segment, offset + 10 * i);

		int number = x87_register_number(emu, i);
#if _SUPPORT_FLOAT80
		emu->x87.bank[emu->x87.current_bank].fpr[number].isfp = true;
#endif
		emu->x87.bank[emu->x87.current_bank].fpr[number].f = fpr;
		if((abridged_tw & (1 << number)) != 0)
			x87_tag_set(emu, i, X87_TAG_EMPTY);
		else
			x87_register_tag(emu, i);
	}
	for(int i = 0; i < xmm_count; i++)
	{
		x86_memory_segmented_read128(emu, segment, offset + 0x00A0 + 16 * i, &emu->xmm[i]);
	}
}

static inline void x87_state_save_extended32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_save_extended_common(emu, segment, offset, 8);
	x86_memory_segmented_write32(emu, segment, offset + 0x0008, emu->x87.fip);
	x86_memory_segmented_write16(emu, segment, offset + 0x000C, emu->x87.fcs);
	x86_memory_segmented_write32(emu, segment, offset + 0x0010, emu->x87.fdp);
	x86_memory_segmented_write16(emu, segment, offset + 0x0014, emu->x87.fds);
}

static inline void x87_state_restore_extended32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_restore_extended_common(emu, segment, offset, 8);
	emu->x87.fip = x86_memory_segmented_read32(emu, segment, offset + 0x0008);
	emu->x87.fcs = x86_memory_segmented_read16(emu, segment, offset + 0x000C);
	emu->x87.fdp = x86_memory_segmented_read32(emu, segment, offset + 0x0010);
	emu->x87.fds = x86_memory_segmented_read16(emu, segment, offset + 0x0014);
}

static inline void x87_state_save_extended32_64(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_save_extended_common(emu, segment, offset, 16);
	x86_memory_segmented_write32(emu, segment, offset + 0x0008, emu->x87.fip);
	x86_memory_segmented_write16(emu, segment, offset + 0x000C, emu->x87.fcs);
	x86_memory_segmented_write32(emu, segment, offset + 0x0010, emu->x87.fdp);
	x86_memory_segmented_write16(emu, segment, offset + 0x0014, emu->x87.fds);
}

static inline void x87_state_restore_extended32_64(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_restore_extended_common(emu, segment, offset, 16);
	emu->x87.fip = x86_memory_segmented_read32(emu, segment, offset + 0x0008);
	emu->x87.fcs = x86_memory_segmented_read16(emu, segment, offset + 0x000C);
	emu->x87.fdp = x86_memory_segmented_read32(emu, segment, offset + 0x0010);
	emu->x87.fds = x86_memory_segmented_read16(emu, segment, offset + 0x0014);
}

static inline void x87_state_save_extended64(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_save_extended_common(emu, segment, offset, 16);
	x86_memory_segmented_write64(emu, segment, offset + 0x0008, emu->x87.fip);
	x86_memory_segmented_write64(emu, segment, offset + 0x0010, emu->x87.fdp);
}

static inline void x87_state_restore_extended64(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_state_restore_extended_common(emu, segment, offset, 16);
	emu->x87.fip = x86_memory_segmented_read64(emu, segment, offset + 0x0008);
	emu->x87.fdp = x86_memory_segmented_read64(emu, segment, offset + 0x0010);
}

static inline void x87_store_exception_pointers(x86_state_t * emu)
{
	emu->x87.fop = emu->x87.next_fop;
	emu->x87.fcs = emu->x87.next_fcs;
	emu->x87.fip = emu->x87.next_fip;
	emu->x87.fds = emu->x87.next_fds;
	emu->x87.fdp = emu->x87.next_fdp;
}


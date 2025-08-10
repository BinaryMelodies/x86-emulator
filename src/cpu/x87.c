
//// Floating point and x87 support

#if _SUPPORT_FLOAT80
# include <fenv.h>
#endif

enum
{
	FP80_NAN_QUIET,
	FP80_INFINITE,
	FP80_ZERO,
	FP80_SUBNORMAL,
	FP80_NORMAL,
	// 8087/287 specific values
	FP80_PSEUDO_NAN,
	FP80_PSEUDO_INFINITE,
	FP80_PSEUDO_ZERO,
	FP80_PSEUDO_SUBNORMAL,
	FP80_UNNORMAL,
	// 387 and later
	FP80_NAN_SIGNALING,
};

static inline x87_float80_t x87_float80_make_indefinite(void)
{
	x87_float80_t result;
#if _SUPPORT_FLOAT80
	result.value = copysignl(NAN, -1.0);
	result.fraction = 0xC000000000000000U;
#else
	result.exponent = 0xFFFF;
	result.fraction = 0xC000000000000000U;
#endif
	return result;
}

static inline x87_float80_t x87_float80_make_infinity(bool sign)
{
	x87_float80_t result;
#if _SUPPORT_FLOAT80
	result.value = copysignl(INFINITY, sign ? -1.0 : 0.0);
	result.fraction = 0x8000000000000000U;
#else
	result.fraction = 0x8000000000000000U;
	result.exponent = 0x7FFF | (sign ? 0x8000 : 0x0000);
#endif
	return result;
}

static inline x87_float80_t x87_float80_make_zero(bool sign)
{
	x87_float80_t result;
#if _SUPPORT_FLOAT80
	result.value = sign ? -0.0 : 0.0;
	result.exponent = 0;
#else
	result.fraction = 0;
	result.exponent = sign ? 0x8000 : 0x0000;
#endif
	return result;
}

#if _SUPPORT_FLOAT80
# define FLOAT80_MAKE(_value, _fraction, _exponent) { .value = (_value), .exponent = (_exponent) }
# define FLOAT80_MAKE_NAN(_payload) { .value = NAN, .fraction = (_payload) }
#else
# define FLOAT80_MAKE(_value, _fraction, _exponent) { .fraction = (_fraction), .exponent = (_exponent) }
# define FLOAT80_MAKE_NAN(_payload) { .fraction = (_payload), .fraction = 0x7FFF }
#endif

const x87_float80_t FLOAT80_ZERO = FLOAT80_MAKE(0.0, 0, 0);
const x87_float80_t FLOAT80_ONE = FLOAT80_MAKE(1.0, 0x8000000000000000U, 0x3FFF);
const x87_float80_t FLOAT80_PI = FLOAT80_MAKE(M_PI, 0xC90FDAA22168C000U /* TODO: this is the 64-bit expansion */, 0x3FFF + 1);
const x87_float80_t FLOAT80_LOG2E = FLOAT80_MAKE(M_LOG2E, 0xB17217F7D1CF7800 /* TODO: this is the 64-bit expansion */, 0x3FFF - 1);
const x87_float80_t FLOAT80_LOG2_10 = FLOAT80_MAKE(M_LOG2E * M_LN10, 0x9A209A84FBCFF000 /* TODO: this is the 64-bit expansion */, 0x3FFF - 2);
const x87_float80_t FLOAT80_LOG10_2 = FLOAT80_MAKE(M_LOG10E * M_LN2, 0xD49A784BCD1B9000 /* TODO: this is the 64-bit expansion */, 0x3FFF + 1);
const x87_float80_t FLOAT80_LN2 = FLOAT80_MAKE(M_LN2, 0xB17217F7D1CF7800 /* TODO: this is the 64-bit expansion */, 0x3FFF - 1);

static inline x87_float80_t x87_float80_make_int64(int64_t value)
{
	x87_float80_t result;
	if(value == 0)
	{
		result = FLOAT80_ZERO;
	}
	else
	{
#if _SUPPORT_FLOAT80
		int exp;
		float80_t tmp = value;
		result.value = frexpl(tmp, &exp);
		result.exponent = exp + 0x3FFE;
#else
		result.fraction = value < 0 ? -value : value;
		result.exponent = 0x3FFF + 63;
		while((result.fraction & 0x8000000000000000U) == 0)
		{
			result.fraction <<= 1;
			result.exponent -= 1;
		}
		if(value < 0)
			result.exponent |= 0x8000;
#endif
	}
	return result;
}

#if _SUPPORT_FLOAT80
static inline x87_float80_t x87_float80_make(float80_t value)
{
	x87_float80_t result;
	int exp;
	switch(fpclassify(value))
	{
	case FP_NAN:
		result = x87_float80_make_indefinite();
		break;
	case FP_INFINITE:
		result = x87_float80_make_infinity(signbit(value));
		break;
	case FP_ZERO:
		result = x87_float80_make_zero(signbit(value));
		break;
	case FP_SUBNORMAL:
#if _SUPPORT_FLOAT80
		result.value = value;
		result.exponent = 0;
#else
		result.fraction = (uint64_t)ldexpl(fabsl(value), 0x3FFE - 1 + 64);
		result.exponent = signbit(value) ? 0x8000 : 0x0000;
#endif
		break;
	case FP_NORMAL:
#if _SUPPORT_FLOAT80
		result.value = value;
		frexpl(value, &exp);
		result.exponent = exp + 0x3FFE + (signbit(value) ? 0x8000 : 0x0000);
#else
		result.fraction = (uint64_t)ldexpl(frexpl(fabsl(value), &exp), 64);
		result.exponent = exp + 0x3FFE + (signbit(value) ? 0x8000 : 0x0000);
#endif
		break;
	}
	return result;
}
#endif

#if _SUPPORT_FLOAT80
static inline x87_float80_t x87_float80_make_8087(float80_t value)
{
	x87_float80_t result;
	int exp;
	switch(fpclassify(value))
	{
	case FP_NAN:
		result = x87_float80_make_indefinite();
		break;
	case FP_INFINITE:
		result.value = INFINITY;
		result.fraction = 0x8000000000000000U;
		break;
	case FP_ZERO:
		result.value = 0.0;
		result.exponent = 0;
		break;
	case FP_SUBNORMAL:
		// load as unnormal
		result.value = value;
		result.exponent = 1;
		break;
	case FP_NORMAL:
		result.value = value;
		frexpl(value, &exp);
		result.exponent = exp + 0x3FFE;
		break;
	}
	return result;
}
#endif

static inline x87_float80_t x87_float80_reduce_precision(x87_float80_t value, unsigned bits);

static inline x87_float80_t x87_make_unnormal(x87_float80_t x87value, uint16_t exponent)
{
	if(x87value.exponent < exponent && x87value.exponent != 0)
	{
#if _SUPPORT_FLOAT80
		// clear lowest (exponent - actual_exponent) bits
		x87value = x87_float80_reduce_precision(x87value, 64 - (exponent - x87value.exponent));
#else
		x87value.fraction >>= (exponent - x87value.exponent);
#endif
		x87value.exponent = exponent;
	}
	return x87value;
}

static inline bool signbit80(x87_float80_t x87value)
{
#if _SUPPORT_FLOAT80
	return signbit(x87value.value);
#else
	return x87value.exponent & 0x8000;
#endif
}

static inline int fp80classify(x87_float80_t x87value)
{
#if _SUPPORT_FLOAT80
	switch(fpclassify(x87value.value))
	{
	case FP_NAN:
		if((x87value.fraction & 0x8000000000000000U) == 0)
			return FP80_PSEUDO_NAN;
		else if((x87value.fraction & 0x4000000000000000U) == 0)
			return FP80_NAN_QUIET;
		else
			return FP80_NAN_SIGNALING;
	case FP_INFINITE:
		if((x87value.fraction & 0x8000000000000000U) == 0)
			return FP80_PSEUDO_INFINITE;
		else
			return FP80_INFINITE;
	case FP_ZERO:
		if(x87value.exponent == 0)
			return FP80_ZERO;
		else
			return FP80_PSEUDO_ZERO;
	case FP_SUBNORMAL:
		if(x87value.exponent == 0)
			return FP80_SUBNORMAL;
		else
			return FP80_PSEUDO_SUBNORMAL;
	case FP_NORMAL:
		if(x87value.exponent == 0)
			return FP80_PSEUDO_SUBNORMAL;
		else
		{
			int exp;
			frexpl(x87value.value, &exp);
			if(x87value.exponent == exp + 0x3FFE)
				return FP80_NORMAL;
			else
				return FP80_UNNORMAL;
		}
	default:
		assert(false);
	}
#else
	if((x87value.exponent & 0x7FFF) == 0)
	{
		if(x87value.fraction == 0)
			return FP80_ZERO;
		else if((x87value.fraction & 0x8000000000000000U) == 0)
			return FP80_PSEUDO_SUBNORMAL;
		else
			return FP80_SUBNORMAL;
	}
	else if((x87value.exponent & 0x7FFF) == 0x7FFF)
	{
		if(x87value.fraction == 0)
			return FP80_PSEUDO_INFINITE;
		else if(x87value.fraction == 0x8000000000000000U)
			return FP80_INFINITE;
		else if((x87value.fraction & 0x8000000000000000U) == 0)
			return FP80_PSEUDO_NAN;
		else if((x87value.fraction & 0x4000000000000000U) == 0)
			return FP80_NAN_QUIET;
		else
			return FP80_NAN_SIGNALING;
	}
	else
	{
		if(x87value.fraction == 0)
			return FP80_PSEUDO_ZERO;
		else if((x87value.fraction & 0x8000000000000000U) == 0)
			return FP80_UNNORMAL;
		else
			return FP80_NORMAL;
	}
#endif
}

static inline bool isinf80(x87_float80_t x87value)
{
	return fp80classify(x87value) == FP80_INFINITE || fp80classify(x87value) == FP80_PSEUDO_INFINITE;
}

static inline bool isnan80(x87_float80_t x87value)
{
	return fp80classify(x87value) == FP80_NAN_QUIET || fp80classify(x87value) == FP80_NAN_SIGNALING || fp80classify(x87value) == FP80_PSEUDO_NAN;
}

static inline bool issubnormal80(x87_float80_t x87value)
{
	return fp80classify(x87value) == FP80_SUBNORMAL; // TODO: what about pseudo-subnormals?
}

static inline x87_float80_t make_quiet_nan(x87_float80_t x87value)
{
	// Note: this assumes that x87value is a NaN
	x87value.fraction &= 0x4000000000000000U;
	return x87value;
}

static inline x87_float80_t x87_make_quiet_nan(x86_state_t * emu, x87_float80_t x87value)
{
	if(emu->x87.fpu_type < X87_FPU_387)
		// 8087/287 do not convert NaNs into quiet NaNs
		return x87value;
	else
		return make_quiet_nan(x87value);
}

static inline unsigned x87_determine_tag(x86_state_t * emu, x87_float80_t x87value)
{
	switch(fp80classify(x87value))
	{
	case FP80_NAN_QUIET:
	case FP80_NAN_SIGNALING:
	case FP80_PSEUDO_NAN:
	case FP80_INFINITE:
	case FP80_PSEUDO_INFINITE:
	case FP80_SUBNORMAL:
	case FP80_PSEUDO_SUBNORMAL:
		return X87_TAG_SPECIAL;
	case FP80_ZERO:
		return X87_TAG_ZERO;
	case FP80_PSEUDO_ZERO:
		if(emu->x87.fpu_type < X87_FPU_387)
			return X87_TAG_VALID;
		else
			return X87_TAG_SPECIAL;
	case FP80_UNNORMAL:
		if(emu->x87.fpu_type < X87_FPU_387)
			return X87_TAG_VALID;
		else
			return X87_TAG_SPECIAL;
	case FP80_NORMAL:
		return X87_TAG_VALID;
	default:
		assert(false);
	}
}

#if _SUPPORT_FLOAT80
static inline unsigned x87_determine_tag_nonfp(x86_state_t * emu, uint64_t fraction, uint16_t exponent)
{
	(void) emu;
	exponent &= 0x7FFF;
	if(exponent == 0x7FFF)
	{
		//case FP80_NAN_QUIET:
		//case FP80_NAN_SIGNALING:
		//case FP80_PSEUDO_NAN:
		//case FP80_INFINITE:
		//case FP80_PSEUDO_INFINITE:
		return X87_TAG_SPECIAL;
	}
	else if(exponent == 0)
	{
		if(fraction == 0)
			//case FP80_ZERO:
			return X87_TAG_ZERO;
		else
			//case FP80_SUBNORMAL:
			//case FP80_PSEUDO_SUBNORMAL:
			return X87_TAG_SPECIAL;
	}
	else
	{
		if((fraction & 0x8000000000000000U) == 0)
			//case FP80_NORMAL:
			return X87_TAG_VALID;
		else
			//case FP80_PSEUDO_ZERO:
			//case FP80_UNNORMAL:
			return X87_TAG_SPECIAL;
	}
}
#endif

static inline void x87_convert_from_float80(x87_float80_t x87value, uint64_t * fraction, uint16_t * exponent, bool * sign)
{
#if _SUPPORT_FLOAT80
	int exp;
	switch(fpclassify(x87value.value))
	{
	case FP_NAN:
		// also pseudo-NaNs
		*fraction = x87value.fraction;
		*exponent = 0x7FFF;
		break;
	case FP_INFINITE:
		// also pseudo-infinity
		*fraction = x87value.fraction;
		*exponent = 0x7FFF;
		break;
	case FP_ZERO:
		// also pseudo-zeroes
		*fraction = 0;
		*exponent = x87value.exponent;
		break;
	case FP_SUBNORMAL:
		*fraction = (uint64_t)ldexpl(fabsl(x87value.value), 0x3FFE - 1 + 64);
		if(x87value.exponent != 0)
		{
			// unnormal
			*fraction >>= (x87value.exponent - 1);
			*exponent = x87value.exponent;
		}
		else
		{
			*exponent = 0;
		}
		break;
	case FP_NORMAL:
		*fraction = (uint64_t)ldexpl(frexpl(fabsl(x87value.value), &exp), 64);
		*exponent = exp + 0x3FFE;

		if(x87value.exponent == 0)
		{
			// pseudo-subnormal
			// Note: exponent is *expected to be 1
			*exponent = x87value.exponent;
		}
		else if(x87value.exponent != *exponent)
		{
			// unnormal
			*fraction >>= (x87value.exponent - *exponent);
			*exponent = x87value.exponent;
		}
		break;
	}
#else
	*fraction = x87value.fraction;
	*exponent = x87value.exponent & 0x7FFF;
#endif
	*sign = signbit80(x87value);
}

static inline x87_float80_t x87_convert_to_float80(uint64_t fraction, uint16_t exponent, bool sign)
{
	x87_float80_t result;
#if _SUPPORT_FLOAT80
	exponent &= 0x7FFF;
	if(exponent == 0x7FFF)
	{
		if((fraction & ~0x8000000000000000U) == 0)
		{
			// infinity, pseudo-infinity
			result.value = copysignl(INFINITY, sign ? -1.0 : 1.0);
			result.fraction = fraction;
		}
		else
		{
			// NaN, pseudo-NaN
			result.value = copysignl((float80_t)NAN, sign ? -1.0 : 1.0);
			result.fraction = fraction;
		}
	}
	else if(exponent == 0)
	{
		// pseudo-denormal, denormal, zero
		result.value = ldexpl(copysignl((float80_t)fraction, sign ? -1.0 : 1.0), (int)0x0001 - (0x3FFE + 64));
		result.exponent = 0;
	}
	else
	{
		// normal, unnormal, pseudo-zero
		result.value = ldexpl(copysignl((float80_t)fraction, sign ? -1.0 : 1.0), (int)exponent - (0x3FFE + 64));
		result.exponent = exponent;
	}
#else
	result.fraction = fraction;
	result.exponent = (exponent & 0x7FFF) | (sign ? 0x8000 : 0x0000);
#endif
	return result;
}

#if _SUPPORT_FLOAT80
static inline int x87_get_std_rounding_mode(x86_state_t * emu)
{
	switch((emu->x87.cw & X87_CW_RC_MASK) >> X87_CW_RC_SHIFT)
	{
	case X87_RC_NEAREST:
		return FE_TONEAREST;
	case X87_RC_DOWN:
		return FE_DOWNWARD;
	case X87_RC_UP:
		return FE_UPWARD;
	case X87_RC_ZERO:
		return FE_TOWARDZERO;
	default:
		assert(false);
	}
}
#endif

static inline void x87_signal_exception(x86_state_t * emu, int intnum);

// used for accessing memory
static inline x87_float80_t x87_convert32_to_float(x86_state_t * emu, uint32_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;

	// TODO: floating point exceptions

	exponent = (value & 0x7F800000) >> 23;
	if(exponent == 0)
	{
		fraction = (uint64_t)(value & 0x007FFFFF);
		if(fraction == 0)
		{
			// zero
			exponent = 0;
		}
		else
		{
			// subnormal
			x87_signal_exception(emu, X87_SW_DE);

			if(emu->x87.fpu_type < X87_FPU_387)
			{
				// load as unnormal
				exponent = 127 - 16383 + 40;
			}
			else
			{
				// load as normal
				exponent = 127 - 16383 + 40;
				while((fraction & 0x8000000000000000) == 0)
				{
					exponent --;
					fraction <<= 1;
				}
			}
		}
	}
	else
	{
		fraction = value & 0x007FFFFF;
		if(exponent == 0xFF)
		{
			// infinite and NaNs
			if(emu->x87.fpu_type >= X87_FPU_387 && fraction != 0 && (fraction & 0x00400000))
			{
				// signalling NaN
				x87_signal_exception(emu, X87_SW_IE);
			}

			exponent = 0x7FFF;
		}
		else
		{
			exponent += 127 - 16383;
		}
		fraction = (fraction << 40) | 0x8000000000000000;
	}
	sign = (value & 0x80000000) != 0;

	return x87_convert_to_float80(fraction, exponent, sign);
}

// used for accessing memory
static inline x87_float80_t x87_convert64_to_float(x86_state_t * emu, uint64_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;

	exponent = (value & 0x7FF0000000000000) >> 52;
	if(exponent == 0)
	{
		fraction = (value & 0x000FFFFFFFFFFFFF) << 11;
		if(fraction == 0)
		{
			// zero
			exponent = 0;
		}
		else
		{
			// subnormal
			x87_signal_exception(emu, X87_SW_DE);

			if(emu->x87.fpu_type < X87_FPU_387)
			{
				// load as unnormal
				exponent = 1023 - 16383 + 11;
			}
			else
			{
				// load as normal
				exponent = 1023 - 16383 + 11;
				while((fraction & 0x8000000000000000) == 0)
				{
					exponent --;
					fraction <<= 1;
				}
			}
		}
	}
	else
	{
		fraction = value & 0x000FFFFFFFFFFFFF;
		if(exponent == 0x7FF)
		{
			// infinite and NaNs
			if(emu->x87.fpu_type >= X87_FPU_387 && fraction != 0 && (fraction & 0x0008000000000000))
			{
				// signalling NaN
				x87_signal_exception(emu, X87_SW_IE);
			}

			exponent = 0x7FFF;
		}
		else
		{
			exponent += 1023 - 16383;
		}
		fraction = (fraction << 11) | 0x8000000000000000;
	}
	sign = (value & 0x8000000000000000) != 0;

	return x87_convert_to_float80(fraction, exponent, sign);
}

static inline x87_float80_t x87_float80_reduce_precision(x87_float80_t value, unsigned bits)
{
	switch(fp80classify(value))
	{
	case FP80_SUBNORMAL:
	case FP80_NORMAL:
	case FP80_PSEUDO_SUBNORMAL:
	case FP80_UNNORMAL:
#if _SUPPORT_FLOAT80
		{
			int exp;
			float80_t tmp = truncl(ldexpl(frexpl(value.value, &exp), bits));
			value.value = ldexpl(tmp, exp - bits);
		}
#else
		value.fraction &= (uint64_t)-1 << (64 - bits);
#endif
		break;
	}
	return value;
}

static inline x87_float80_t x87_float80_round24(x87_float80_t value)
{
	// TODO: floating point exceptions

	switch(fp80classify(value))
	{
	case FP80_SUBNORMAL:
	case FP80_NORMAL:
	case FP80_PSEUDO_SUBNORMAL:
	case FP80_UNNORMAL:
#if _SUPPORT_FLOAT80
# if _FLOAT32_EXACT
		value.value = (float32_t)value.value;
# else
		{
			// use rounding to restrict precision to 24 bits
			int exp;
			float80_t tmp = rintl(ldexpl(frexpl(value.value, &exp), 24));
			value.value = ldexpl(tmp, exp - 24);
		}
# endif
#else
		// TODO
#endif
		break;
	}
	return value;
}

static inline x87_float80_t x87_float80_round53(x87_float80_t value)
{
	// TODO: floating point exceptions

	switch(fp80classify(value))
	{
	case FP80_SUBNORMAL:
	case FP80_NORMAL:
	case FP80_PSEUDO_SUBNORMAL:
	case FP80_UNNORMAL:
#if _SUPPORT_FLOAT80
# if _FLOAT64_EXACT
		value.value = (float64_t)value.value;
# else
		{
			// use rounding to restrict precision to 53 bits
			int exp;
			float80_t tmp = rintl(ldexpl(frexpl(value.value, &exp), 53));
			value.value = ldexpl(tmp, exp - 53);
		}
# endif
#else
		// TODO
#endif
		break;
	}
	return value;
}

static inline uint32_t x87_convert32_from_float(x86_state_t * emu, x87_float80_t value, int tag)
{
	uint32_t result;
	uint64_t fraction;
	uint16_t exponent;
	bool sign;

	if(emu->x87.fpu_type >= X87_FPU_387)
		tag = x87_determine_tag(emu, value);

	switch(tag)
	{
	case X87_TAG_SPECIAL:
		x87_convert_from_float80(value, &fraction, &exponent, &sign);
		fraction = (fraction & 0x7FFFFFFFFFFFFFFF) >> 40;
		exponent = exponent >> 4;
		break;
	default:
		value = x87_float80_round24(value);
		x87_convert_from_float80(value, &fraction, &exponent, &sign);
		if(exponent == 0x7FFF)
		{
			// infinity or NaN
			fraction = (fraction & 0x7FFFFFFFFFFFFFFFU) >> 40; // already rounded
			exponent = 0xFF;
		}
		else if(fraction == 0)
		{
			// zero or pseudo-zero
			exponent = 0;
		}
		else
		{
			// normalize
			while(exponent > 1 && (fraction & 0x8000000000000000U) == 0)
			{
				exponent -= 1;
				fraction <<= 1;
			}

			if(exponent >= 0xFF + 16383 - 127)
			{
				// TODO: overflow
				// convert to infinity
				fraction = 0;
				exponent = 0xFF;
			}
			else if(exponent <= 16383 - 127)
			{
				// convert to subnormal or zero
				// TODO: underflow
				if(40 + 1 + 16383 - 127 - exponent < 64)
					fraction >>= 40 + 1 + 16383 - 127 - exponent;
				else
					fraction = 0;
				exponent = 0;
			}
			else
			{
				fraction = (fraction & 0x7FFFFFFFFFFFFFFFU) >> 40; // already rounded
				exponent = exponent - 16383 + 127;
			}
		}
		break;
	}

	result = fraction | (uint32_t)exponent << 23;
	if(sign)
		result |= 0x80000000;
	return result;
}

static inline uint64_t x87_convert64_from_float(x86_state_t * emu, x87_float80_t value, int tag)
{
	uint64_t result;
	uint64_t fraction;
	uint16_t exponent;
	bool sign;

	if(emu->x87.fpu_type >= X87_FPU_387)
		tag = x87_determine_tag(emu, value);

	switch(tag)
	{
	case X87_TAG_SPECIAL:
		x87_convert_from_float80(value, &fraction, &exponent, &sign);
		fraction = (fraction & 0x7FFFFFFFFFFFFFFF) >> 11;
		exponent = exponent >> 4;
		break;
	default:
		value = x87_float80_round53(value);
		x87_convert_from_float80(value, &fraction, &exponent, &sign);
		if(exponent == 0x7FFF)
		{
			// infinity or NaN
			fraction = (fraction & 0x7FFFFFFFFFFFFFFFU) >> 11; // already rounded
			exponent = 0x7FF;
		}
		else if(fraction == 0)
		{
			// zero or pseudo-zero
			exponent = 0;
		}
		else
		{
			// normalize
			while(exponent > 1 && (fraction & 0x8000000000000000U) == 0)
			{
				exponent -= 1;
				fraction <<= 1;
			}

			if(exponent >= 0x7FF + 16383 - 1023)
			{
				// TODO: overflow
				// convert to infinity
				fraction = 0;
				exponent = 0x7FF;
			}
			else if(exponent <= 16383 - 1023)
			{
				// convert to subnormal or zero
				// TODO: underflow
				if(11 + 1 + 16383 - 1023 - exponent < 64)
					fraction >>= 11 + 1 + 16383 - 1023 - exponent;
				else
					fraction = 0;
				exponent = 0;
			}
			else
			{
				fraction = (fraction & 0x7FFFFFFFFFFFFFFFU) >> 11; // already rounded
				exponent = exponent - 16383 + 1023;
			}
		}
		break;
	}

	result = fraction | (uint64_t)exponent << 52;
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

static inline void x87_signal_exception(x86_state_t * emu, int intnum)
{
	emu->x87.sw |= intnum;

	if(emu->x87.fpu_type >= X87_FPU_287)
	{
		emu->x87.sw |= X87_SW_ES;
		if(emu->x87.fpu_type == X87_FPU_INTEGRATED)
			emu->x87.sw |= X87_SW_B;
	}

	if((emu->x87.cw & intnum) == 0)
	{
		if(emu->x87.fpu_type == X87_FPU_8087)
		{
			emu->x87.sw |= X87_SW_IR; // same position as the X87_SW_ES flag
			x87_trigger_interrupt(emu);
		}
		else
		{
			// later FPUs only trigger the interrupt when encountering an x87 instruction or WAIT
			longjmp(emu->exc[emu->fetch_mode], 1);
		}
	}
}

static inline void x87_signal_stack_overflow(x86_state_t * emu)
{
	if(emu->x87.fpu_type >= X87_FPU_387)
	{
		emu->x87.sw |= X87_SW_SF;
		emu->x87.sw |= X87_SW_C1;
	}
	x87_signal_exception(emu, X87_SW_IE);
}

static inline void x87_signal_stack_underflow(x86_state_t * emu)
{
	if(emu->x87.fpu_type >= X87_FPU_387)
	{
		emu->x87.sw |= X87_SW_SF;
		emu->x87.sw &= ~X87_SW_C1;
	}
	x87_signal_exception(emu, X87_SW_IE);
}

static inline int64_t x87_float80_to_int64(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
#if _SUPPORT_FLOAT80
	return value.value;
#else
	int64_t result;
	uint32_t exponent = value.exponent & 0x7FFF;
	if(exponent > 0x3FFE + 63 + 63)
		return (uint64_t)-1 << 63; // indefinite
	else if(exponent > 0x3FFE + 63)
		result = value.fraction << (exponent - (0x3FFE + 63));
	else if(exponent == 0x3FFE + 63)
		result = value.fraction;
	else if(exponent > 0x3FFE + 63 - 64)
		result = value.fraction >> (64 - (exponent - (0x3FFE + 63)));
	else
		return 0;
	if((value.exponent & 0x8000) != 0)
		return -result;
	else
		return result;
#endif
}

static inline x87_float80_t x87_int64_to_float80(x86_state_t * emu, int64_t value)
{
	(void) emu;
	return x87_float80_make_int64(value);
}

static inline x87_float80_t x87_packed80_to_float80(const uint8_t bytes[10])
{
	int64_t int_value = 0;
	x87_float80_t result;
	for(int digit_pair = 0, tens = 1; digit_pair < 9; digit_pair++, tens *= 100)
	{
		int_value +=
			tens * (bytes[digit_pair] & 0xF)
			+ tens * 10 * ((bytes[digit_pair] >> 4) & 0xF);
	}
	result = x87_float80_make_int64(int_value);
	if((bytes[0] & 0x80) == 0)
	{
#if _SUPPORT_FLOAT80
		result.value = -result.value;
#else
		result.exponent ^= 0x8000;
#endif
	}
	return result;
}

static inline void x87_float80_to_packed80(x87_float80_t value, uint8_t bytes[10])
{
	int64_t int_value;
	bool sign;
#if _SUPPORT_FLOAT80
	// TODO: infinity and NaN
	int_value = (int64_t)(value.value + 0.5);
	if(int_value < 0)
		int_value = -int_value;
	sign = value.value < 0;
	bytes[0] |= value.value < 0 ? 0x80 : 0;
#else
	sign = value.exponent & 0x8000;
	value.exponent &= 0x7FFF;
	if(value.exponent < 0x3FFE + 64)
	{
		// TODO: check for rounding errors
		int_value = value.fraction >> (0x3FFE + 64 - value.exponent); // TODO: check if too much shift
	}
	else if(value.exponent > 0x3FFE + 64)
	{
		// TODO: check for rounding errors
		int_value = value.fraction << (value.exponent - (0x3FFE + 64)); // TODO: check if too much shift
	}
#endif
	for(int digit_pair = 0; digit_pair < 9; digit_pair++)
	{
		bytes[digit_pair] = (int_value % 10) + (((int_value / 10) % 10) << 4);
	}
	bytes[0] |= sign ? 0x80 : 0;
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

static inline int x87_tag_get_direct(x86_state_t * emu, int number)
{
	number &= 7;
	return (emu->x87.tw >> (number * 2)) & 3;
}

static inline void x87_tag_set_direct(x86_state_t * emu, int number, int tag)
{
	number &= 7;
	emu->x87.tw = (emu->x87.tw & ~(3 << (number * 2))) | ((tag & 3) << (number * 2));
}

static inline int x87_tag_get(x86_state_t * emu, int number)
{
	return x87_tag_get_direct(emu, x87_register_number(emu, number));
}

static inline void x87_tag_set(x86_state_t * emu, int number, int tag)
{
	x87_tag_set_direct(emu, x87_register_number(emu, number), tag);
}

static inline x87_float80_t x87_restrict_precision(x86_state_t * emu, x87_float80_t value)
{
	switch((emu->x87.cw >> X87_CW_PC_SHIFT) & 3)
	{
	case 0:
		return x87_float80_round24(value);
	case 2:
		return x87_float80_round53(value);
	case 1:
		// reserved
	case 3:
		return value;
	default:
		assert(false);
	}
}

static inline void x87_store_register_bank(x86_state_t * emu)
{
	for(size_t number = 0; number < 8; number++)
	{
		emu->x87.bank[emu->x87.current_bank].fpr[number] = emu->x87.fpr[number];
	}
}

static inline void x87_load_register_bank(x86_state_t * emu)
{
	for(size_t number = 0; number < 8; number++)
	{
		emu->x87.fpr[number] = emu->x87.bank[emu->x87.current_bank].fpr[number];
	}
}

static inline void x87_set_register_bank(x86_state_t * emu, unsigned bank_number)
{
	if(emu->x87.current_bank == bank_number)
		return;

	x87_store_register_bank(emu);
	emu->x87.current_bank = bank_number;
	x87_load_register_bank(emu);
}

static inline x87_register_t * x87_register(x86_state_t * emu, x86_regnum_t number)
{
	return &emu->x87.fpr[x87_register_number(emu, number)];
}

static inline x87_register_t * x87_register_bank(x86_state_t * emu, x86_regnum_t number, unsigned bank_number)
{
	return &emu->x87.bank[bank_number].fpr[x87_register_number(emu, number)];
}

static inline x87_float80_t x87_register_get80_no_exception_check(x86_state_t * emu, x86_regnum_t number)
{
#if _SUPPORT_FLOAT80
	if(x87_register(emu, number)->isfp)
	{
		return x87_register(emu, number)->f;
	}
	else
	{
		// stored as MMX register, convert it to floating point representation on the fly
		uint16_t exponent = x87_register(emu, number)->exponent;
		return x87_convert_to_float80(x87_register(emu, number)->mmx.q[0], exponent & 0x7FFF, (exponent & 0x8000) != 0);
	}
#else
	return x87_register(emu, number)->f;
#endif
}

static inline x87_float80_t x87_register_get80(x86_state_t * emu, x86_regnum_t number)
{
	if(x87_tag_get(emu, number) == X87_TAG_EMPTY)
		x87_signal_stack_underflow(emu);

#if _SUPPORT_FLOAT80
	if(x87_register(emu, number)->isfp)
	{
		return x87_register(emu, number)->f;
	}
	else
	{
		// stored as MMX register, convert it to floating point representation on the fly
		uint16_t exponent = x87_register(emu, number)->exponent;
		return x87_convert_to_float80(x87_register(emu, number)->mmx.q[0], exponent & 0x7FFF, (exponent & 0x8000) != 0);
	}
#else
	return x87_register(emu, number)->f;
#endif
}

static inline x87_float80_t x87_register_get80_bank(x86_state_t * emu, x86_regnum_t number, unsigned bank_number)
{
	if(emu->x87.current_bank == bank_number)
		// current bank is not stored in the bank array
		return x87_register_get80(emu, number);

	if(x87_tag_get(emu, number) == X87_TAG_EMPTY)
		x87_signal_stack_underflow(emu);

#if _SUPPORT_FLOAT80
	if(x87_register_bank(emu, number, bank_number)->isfp)
	{
		return x87_register_bank(emu, number, bank_number)->f;
	}
	else
	{
		// stored as MMX register, convert it to floating point representation on the fly
		uint16_t exponent = x87_register_bank(emu, number, bank_number)->exponent;
		return x87_convert_to_float80(x87_register_bank(emu, number, bank_number)->mmx.q[0], exponent & 0x7FFF, (exponent & 0x8000) != 0);
	}
#else
	return x87_register_bank(emu, number, bank_number)->f;
#endif
}

static inline void x87_register_tag(x86_state_t * emu, x86_regnum_t number)
{
#if _SUPPORT_FLOAT80
	if(!x87_register(emu, number)->isfp)
		// stored as MMX register, convert it to floating point representation on the fly
		x87_tag_set(emu, number, x87_determine_tag_nonfp(emu, x87_register(emu, number)->mmx.q[0], x87_register(emu, number)->exponent));
	else
#endif
		x87_tag_set(emu, number, x87_determine_tag(emu, x87_register(emu, number)->f));
}

static inline void x87_register_set80(x86_state_t * emu, x86_regnum_t number, x87_float80_t value)
{
	value = x87_restrict_precision(emu, value);
#if _SUPPORT_FLOAT80
	// store as floating point register
	x87_register(emu, number)->isfp = true;
#endif
	x87_register(emu, number)->f = value;
	x87_tag_set(emu, number, x87_determine_tag(emu, x87_register(emu, number)->f));
}

static inline void x87_register_set80_bank(x86_state_t * emu, x86_regnum_t number, unsigned bank_number, x87_float80_t value)
{
	if(emu->x87.current_bank == bank_number)
	{
		// current bank is not stored in the bank array
		x87_register_set80(emu, number, value);
		return;
	}

	value = x87_restrict_precision(emu, value);
	number = x87_register_number(emu, number);
#if _SUPPORT_FLOAT80
	// store as floating point register
	x87_register_bank(emu, number, bank_number)->isfp = true;
#endif
	x87_register_bank(emu, number, bank_number)->f = value;
	x87_tag_set(emu, number, x87_determine_tag(emu, x87_register_bank(emu, number, bank_number)->f));
}

static inline void x87_register_free(x86_state_t * emu, x86_regnum_t number)
{
	x87_tag_set(emu, number, X87_TAG_EMPTY);
}

static inline x87_float80_t x87_pop(x86_state_t * emu)
{
	if(x87_tag_get(emu, 0) == X87_TAG_EMPTY)
		x87_signal_stack_underflow(emu);

	x87_float80_t value = x87_register_get80(emu, 0);
	x87_register_free(emu, 0);
	x87_set_sw_top(emu, x87_get_sw_top(emu) + 1);
	return value;
}

static inline void x87_push(x86_state_t * emu, x87_float80_t value)
{
	if(x87_tag_get(emu, 7) != X87_TAG_EMPTY)
		x87_signal_stack_overflow(emu);

	x87_set_sw_top(emu, x87_get_sw_top(emu) - 1);
	x87_register_set80(emu, 0, value);
}

static inline uint64_t x86_mmx_get(x86_state_t * emu, x86_regnum_t number)
{
#if _SUPPORT_FLOAT80
	if(emu->x87.fpr[number].isfp)
	{
		// stored as floating point register, convert to MMX representation on the fly
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
	return emu->x87.fpr[number].mmx.q[0];
#endif
}

static inline void x86_mmx_set(x86_state_t * emu, x86_regnum_t number, uint64_t value)
{
#if _SUPPORT_FLOAT80
	// store as MMX register
	emu->x87.fpr[number].isfp = false;
#endif
	emu->x87.fpr[number].exponent = 0xFFFF;
	emu->x87.fpr[number].mmx.q[0] = value;
}

static inline x87_float80_t x87_f2xm1(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
#if _SUPPORT_FLOAT80
	return x87_float80_make(expm1l(log2l(value.value))); // TODO
#else
	// TODO
	(void) value;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_check_subnormal(x86_state_t * emu, x87_float80_t value)
{
	if(issubnormal80(value))
		x87_signal_exception(emu, X87_SW_DE);
	return value;
}

// used for FLD
static inline x87_float80_t x87_check_subnormal_8087(x86_state_t * emu, x87_float80_t value)
{
	if(emu->x87.fpu_type < X87_FPU_387)
	{
		if(issubnormal80(value))
			x87_signal_exception(emu, X87_SW_DE);
	}
	return value;
}

static inline x87_float80_t x87_check_invalid(x86_state_t * emu, x87_float80_t value)
{
	bool invalid;

	if(emu->x87.fpu_type < X87_FPU_387)
		invalid = isnan80(value);
	else
		invalid = fp80classify(value) == FP80_NAN_SIGNALING;

	if(invalid)
		x87_signal_exception(emu, X87_SW_IE);

	return value;
}

static inline x87_float80_t x87_fabs(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
#if _SUPPORT_FLOAT80
	value.value = fabsl(value.value);
#else
	value.exponent &= 0x7FFF;
#endif
	return value;
}

static inline x87_float80_t x87_fadd(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	x87_float80_t result;

	x87_check_subnormal_8087(emu, value1);
	x87_check_subnormal_8087(emu, value2);

	x87_check_invalid(emu, value1);
	x87_check_invalid(emu, value2);

	x87_check_subnormal(emu, value1);
	x87_check_subnormal(emu, value2);

#if _SUPPORT_FLOAT80
	if(emu->x87.fpu_type < X87_FPU_387)
	{
		// 8087/287 converts subnormals to unnormal
		if(fp80classify(value1) == FP80_SUBNORMAL)
			value1 = x87_float80_make_8087(value1.value);
		if(fp80classify(value2) == FP80_SUBNORMAL)
			value2 = x87_float80_make_8087(value2.value);

		if(isinf80(value1) && isinf80(value2) && (emu->x87.cw & X87_CW_IC) == 0)
		{
			// infinity control makes inf + inf invalid
			x87_signal_exception(emu, X87_SW_IE);

			return x87_float80_make_indefinite();
		}
	}

	if(isnan80(value1) && isnan80(value2))
	{
		// takes NaN of greater magnitude
		// on 387 and later, quiet NaNs override signaling NaNs, but since quiet NaNs have greater magnitude, this works out the same way
		if(value1.fraction > value2.fraction)
			return x87_make_quiet_nan(emu, value1);
		else
			return x87_make_quiet_nan(emu, value2);
	}
	else if(isnan80(value1))
	{
		return x87_make_quiet_nan(emu, value1);
	}
	else if(isnan80(value2))
	{
		return x87_make_quiet_nan(emu, value2);
	}

	fesetround(x87_get_std_rounding_mode(emu));
	feclearexcept(FE_ALL_EXCEPT);

	if(emu->x87.fpu_type < X87_FPU_387)
	{
		result = x87_restrict_precision(emu, x87_float80_make_8087(value1.value + value2.value));

		if(fp80classify(value1) == FP80_UNNORMAL || fp80classify(value2) == FP80_UNNORMAL)
		{
			result = x87_make_unnormal(result, min(value1.exponent, value2.exponent));
		}
	}
	else
	{
		result = x87_restrict_precision(emu, x87_float80_make(value1.value + value2.value));
	}

	if(fetestexcept(FE_OVERFLOW))
		// TODO: 8087/287 behaves differently to the IEEE standard
		// TODO: unmasked response
		x87_signal_exception(emu, X87_SW_OE);

	if(fetestexcept(FE_UNDERFLOW))
		// TODO: 8087/287 behaves differently to the IEEE standard
		// TODO: unmasked response
		x87_signal_exception(emu, X87_SW_UE);

	if(fetestexcept(FE_INEXACT))
		x87_signal_exception(emu, X87_SW_PE);

#else
	// TODO
	(void) emu;
	(void) value1;
	(void) value2;
	result = FLOAT80_ZERO;
#endif
	return result;
}

static inline x87_float80_t x87_fchs(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(-value.value);
#else
	// TODO
	(void) value;
	return FLOAT80_ZERO;
#endif
}

static inline void x87_fcom(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	if(value1.value > value2.value)
	{
		emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
	}
	else if(value1.value > value2.value)
	{
		emu->x87.sw &= ~X87_SW_C3;
		emu->x87.sw |= X87_SW_C0;
	}
	else if(value1.value == value2.value)
	{
		emu->x87.sw |= X87_SW_C3;
		emu->x87.sw &= ~X87_SW_C0;
	}
	else
	{
		emu->x87.sw |= X87_SW_C3;
		emu->x87.sw |= X87_SW_C0;
	}
#else
	// TODO
	(void) value1;
	(void) value2;
#endif
}

static inline x87_float80_t x87_fdiv(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(value1.value / value2.value);
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_fmul(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(value1.value * value2.value);
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_fprem(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	(void) value1;
	(void) value2;
	return x87_float80_make(0.0); // TODO
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline void x87_fptan(x86_state_t * emu, x87_float80_t value, x87_float80_t * result1, x87_float80_t * result2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	// TODO: better emulation of FPTAN
	*result1 = x87_float80_make(1.0);
	*result2 = x87_float80_make(tanl(value.value));
#else
	// TODO
	(void) value;
	(void) result1;
	(void) result2;
#endif
}

static inline x87_float80_t x87_fpatan(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(atan2l(value1.value, value2.value));
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_frndint(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
	// TODO: precision/rounding
	return x87_int64_to_float80(emu, x87_float80_to_int64(emu, value));
}

static inline x87_float80_t x87_fscale(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(ldexpl(value1.value, x87_float80_to_int64(emu, value2)));
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_fsub(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(value1.value - value2.value);
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_fsqrt(x86_state_t * emu, x87_float80_t value)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(sqrtl(value.value));
#else
	// TODO
	(void) value;
	return FLOAT80_ZERO;
#endif
}

static inline void x87_fxam(x86_state_t * emu, x87_float80_t value, bool is_empty)
{
	if(is_empty)
	{
		if(emu->x87.fpu_type < X87_FPU_387)
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
		else
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C2 | X87_SW_C0);
	}
	else
	{
		switch(fp80classify(value))
		{
		case FP80_NAN_SIGNALING:
		case FP80_NAN_QUIET:
		case FP80_PSEUDO_NAN:
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C2);
			emu->x87.sw |= X87_SW_C0;
			break;
		case FP80_UNNORMAL:
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C2 | X87_SW_C0);
			break;
		case FP80_INFINITE:
		case FP80_PSEUDO_INFINITE:
			emu->x87.sw &= ~X87_SW_C3;
			emu->x87.sw |= X87_SW_C2 | X87_SW_C0;
			break;
		case FP80_ZERO:
		case FP80_PSEUDO_ZERO:
			emu->x87.sw &= ~(X87_SW_C2 | X87_SW_C0);
			emu->x87.sw |= X87_SW_C3;
			break;
		case FP80_SUBNORMAL:
		case FP80_PSEUDO_SUBNORMAL:
		case FP80_NORMAL:
			emu->x87.sw &= ~(X87_SW_C3 | X87_SW_C0);
			emu->x87.sw |= X87_SW_C2;
			break;
		}
	}

	if(signbit80(value))
		emu->x87.sw |= X87_SW_C1;
	else
		emu->x87.sw &= ~X87_SW_C1;
}

static inline void x87_fxtract(x86_state_t * emu, x87_float80_t value, x87_float80_t * result1, x87_float80_t * result2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	int exp;
	*result1 = x87_float80_make(frexpl(value.value, &exp));
	*result2 = x87_int64_to_float80(emu, exp);
#else
	// TODO
	(void) value;
	(void) result1;
	(void) result2;
#endif
}

static inline x87_float80_t x87_fyl2x(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(value2.value * log2l(value1.value));
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline x87_float80_t x87_fyl2xp1(x86_state_t * emu, x87_float80_t value1, x87_float80_t value2)
{
	(void) emu;
	// TODO: precision/rounding
#if _SUPPORT_FLOAT80
	return x87_float80_make(value2.value * log2l(value1.value + 1));
#else
	// TODO
	(void) value1;
	(void) value2;
	return FLOAT80_ZERO;
#endif
}

static inline void x87_state_save_registers(x86_state_t * emu, x86_segnum_t segment, uoff_t x86_offset, uoff_t offset)
{
	for(int i = 0; i < 8; i++)
	{
		x87_float80_t fpr = x87_register_get80(emu, i);
		x87_memory_segmented_write80fp(emu, segment, x86_offset, offset + 10 * i, fpr);
	}
}

static inline void x87_state_restore_registers(x86_state_t * emu, x86_segnum_t segment, uoff_t x86_offset, uoff_t offset)
{
	for(int i = 0; i < 8; i++)
	{
		x87_float80_t fpr = x87_memory_segmented_read80fp(emu, segment, x86_offset, offset + 10 * i);

#if _SUPPORT_FLOAT80
		x87_register(emu, i)->isfp = true;
#endif
		x87_register(emu, i)->f = fpr;
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
	// in theory, reading back CS:IP and DS:DP is ambiguous in real mode
	// however it is consistent with 8087 as well as tested Pentium III behavior
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
	x87_state_save_registers(emu, segment, offset, offset + 0x001C);
}

static inline void x87_state_restore_real_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_real_mode32(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x001C);
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
	x87_state_save_registers(emu, segment, offset, offset + 0x001C);
}

static inline void x87_state_restore_protected_mode32(x86_state_t * emu, x86_segnum_t segment, uoff_t offset)
{
	x87_environment_restore_protected_mode32(emu, segment, offset);
	x87_state_restore_registers(emu, segment, offset, offset + 0x001C);
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
	x86_memory_segmented_write32(emu, segment, offset + 0x001C, 0); // TODO: MXCSR_MASK
	for(int i = 0; i < 8; i++)
	{
		x87_float80_t fpr = x87_register_get80(emu, i);
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
		x87_float80_t fpr = x86_memory_segmented_read80fp(emu, segment, offset + 10 * i);

#if _SUPPORT_FLOAT80
		x87_register(emu, i)->isfp = true;
#endif
		x87_register(emu, i)->f = fpr;
		int number = x87_register_number(emu, i);
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
	emu->x87.fds = emu->sr[emu->x87.segment].selector;
	emu->x87.fdp = emu->x87.offset;
}


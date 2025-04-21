
// Related to parsing and interpreting the instruction flow

static inline void x86_parse_modrm16(x86_parser_t * prs, x86_state_t * emu, bool execute)
{
	const char * format;
	int default_segment;
	int disp_size = (prs->modrm_byte >> 6);
	prs->register_field = (prs->modrm_byte >> 3) & 7;

	prs->address_offset = 0;

	switch(prs->modrm_byte & 7)
	{
	case 0:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_BX) + x86_register_get16(emu, X86_R_SI);
		default_segment = X86_R_DS;
		format = prs->use_nec_syntax ? "[%s:bw+ix+%X]" : "[%s:bx+si+%X]";
		break;
	case 1:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_BX) + x86_register_get16(emu, X86_R_DI);
		default_segment = X86_R_DS;
		format = prs->use_nec_syntax ? "[%s:bw+iy+%X]" : "[%s:bx+di+%X]";
		break;
	case 2:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_BP) + x86_register_get16(emu, X86_R_SI);
		default_segment = X86_R_SS;
		format = prs->use_nec_syntax ? "[%s:bp+ix+%X]" : "[%s:bp+si+%X]";
		break;
	case 3:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_BP) + x86_register_get16(emu, X86_R_DI);
		default_segment = X86_R_SS;
		format = prs->use_nec_syntax ? "[%s:bp+iy+%X]" : "[%s:bp+di+%X]";
		break;
	case 4:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_SI);
		default_segment = X86_R_DS;
		format = prs->use_nec_syntax ? "[%s:ix+%X]" : "[%s:si+%X]";
		break;
	case 5:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_DI);
		default_segment = X86_R_DS;
		format = prs->use_nec_syntax ? "[%s:iy+%X]" : "[%s:di+%X]";
		break;
	case 6:
		if(disp_size == 0)
		{
			if(execute)
				emu->parser->address_offset = 0;
			default_segment = X86_R_DS;
			disp_size = 2;
			format = "[%s:%X]";
		}
		else
		{
			if(execute)
				emu->parser->address_offset = x86_register_get16(emu, X86_R_BP);
			default_segment = X86_R_SS;
			format = "[%s:bp+%X]";
		}
		break;
	case 7:
		if(execute)
			emu->parser->address_offset = x86_register_get16(emu, X86_R_BX);
		default_segment = X86_R_DS;
		format = prs->use_nec_syntax ? "[%s:bw+%X]" : "[%s:bx+%X]";
		break;
	}

	switch(disp_size)
	{
	case 1:
		prs->address_offset += (int8_t)x86_fetch8(prs, emu);
		break;
	case 2:
		prs->address_offset += x86_fetch16(prs, emu);
		break;
	}

	prs->address_offset &= 0xFFFF;
	if(prs->segment == NONE)
	{
		prs->segment = default_segment;
	}

	sprintf(prs->address_text, format, x86_segment_name(prs, prs->segment), (int16_t)prs->address_offset);
}

static inline void x86_parse_modrm32(x86_parser_t * prs, x86_state_t * emu, bool execute)
{
	char format[24];
	int default_segment;
	int disp_size = (prs->modrm_byte >> 6);
	int reg = prs->modrm_byte & 7;
	prs->register_field = (prs->modrm_byte >> 3) & 7;

	prs->address_offset = 0;

	if((reg & 7) == 4)
	{
		// SIB byte
		uint8_t sib = x86_fetch8(prs, emu);
		reg = sib & 7;
		int i = (sib >> 3) & 7;
		int s = sib >> 6;

		if(execute)
		{
			if(i == 4)
			{
				emu->parser->address_offset = 0;
			}
			else
			{
				emu->parser->address_offset = x86_register_get32(emu, i) << s;
			}
		}

		if(reg == 5 && disp_size == 0)
		{
			default_segment = X86_R_DS;
			disp_size = 2;
		}
		else
		{
			if(execute)
				emu->parser->address_offset += x86_register_get32(emu, reg);
			default_segment = reg == 4 || reg == 5 ? X86_R_SS : X86_R_DS;
		}

		strcpy(format, "[%s:");
		if(!(reg == 5 && disp_size == 0))
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s+", x86_register_name32(prs, reg));
		}
		if(i != 4)
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s*%d+", x86_register_name32(prs, i), 1 << s);
		}
		strcat(format, "%X]");
	}
	else if((reg & 7) == 5 && disp_size == 0)
	{
		if(execute)
			emu->parser->address_offset = 0;
		default_segment = X86_R_DS;
		disp_size = 2;
		strcpy(format, "[%s:%X]");
	}
	else
	{
		if(execute)
			emu->parser->address_offset = x86_register_get32(emu, reg);
		default_segment = reg == 5 ? X86_R_SS : X86_R_DS;
		snprintf(format, sizeof format, "[%%s:%s+%%X]", x86_register_name32(prs, reg));
	}

	switch(disp_size)
	{
	case 1:
		prs->address_offset += (int8_t)x86_fetch8(prs, emu);
		break;
	case 2:
		prs->address_offset += x86_fetch32(prs, emu);
		break;
	}

	prs->address_offset &= 0xFFFFFFFF;
	if(prs->segment == NONE)
	{
		prs->segment = default_segment;
	}

	sprintf(prs->address_text, format, x86_segment_name(prs, prs->segment), (int32_t)prs->address_offset);
}

static inline void x86_parse_modrm64_32(x86_parser_t * prs, x86_state_t * emu, bool execute)
{
	char format[24];
	int disp_size = (prs->modrm_byte >> 6);
	int reg = prs->modrm_byte & 7;
	int32_t displacement = 0;
	prs->register_field = (prs->modrm_byte >> 3) & 7;
	prs->register_field |= prs->rex_w;
	reg |= prs->rex_b;

	prs->address_offset = 0;

	if((reg & 7) == 4)
	{
		// SIB byte
		uint8_t sib = x86_fetch8(prs, emu);
		reg = sib & 7;
		int i = (sib >> 3) & 7;
		int s = sib >> 6;
		i |= prs->rex_x;
		reg |= prs->rex_b;

		if(execute)
		{
			if(i == 4)
			{
				emu->parser->address_offset = 0;
			}
			else
			{
				emu->parser->address_offset = x86_register_get32(emu, i) << s;
			}
		}

		if((reg & 7) == 5 && disp_size == 0)
		{
			disp_size = 2;
		}
		else
		{
			if(execute)
				emu->parser->address_offset += x86_register_get32(emu, reg);
		}

		strcpy(format, "[%s:");
		if(!((reg & 7) == 5 && disp_size == 0))
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s+", x86_register_name32(prs, reg));
		}
		if(i != 4)
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s*%d+", x86_register_name32(prs, i), 1 << s);
		}
		strcat(format, "%X]");
	}
	else if((reg & 7) == 5 && disp_size == 0)
	{
		prs->ip_relative = true;
		disp_size = 2;
		strcpy(format, "[%s:eip+%X]");
	}
	else
	{
		if(execute)
			emu->parser->address_offset = x86_register_get32(emu, reg);
		snprintf(format, sizeof format, "[%%s:%s+%%X]", x86_register_name32(prs, reg));
	}

	switch(disp_size)
	{
	case 1:
		prs->address_offset += displacement = (int8_t)x86_fetch8(prs, emu);
		break;
	case 2:
		prs->address_offset += displacement = x86_fetch32(prs, emu);
		break;
	}

	prs->address_offset &= 0xFFFFFFFF;
	if(prs->segment == NONE)
	{
		prs->segment = X86_R_DS;
	}

	sprintf(prs->address_text, format, x86_segment_name(prs, prs->segment), (int32_t)displacement);
}

static inline void x86_parse_modrm64(x86_parser_t * prs, x86_state_t * emu, bool execute)
{
	char format[24];
	int disp_size = (prs->modrm_byte >> 6);
	int reg = prs->modrm_byte & 7;
	int32_t displacement = 0;
	prs->register_field = (prs->modrm_byte >> 3) & 7;
	prs->register_field |= prs->rex_w;
	reg |= prs->rex_b;

	prs->address_offset = 0;

	if((reg & 7) == 4)
	{
		// SIB byte
		uint8_t sib = x86_fetch8(prs, emu);
		reg = sib & 7;
		int i = (sib >> 3) & 7;
		int s = sib >> 6;
		i |= prs->rex_x;
		reg |= prs->rex_b;

		if(execute)
		{
			if(i == 4)
			{
				emu->parser->address_offset = 0;
			}
			else
			{
				emu->parser->address_offset = x86_register_get64(emu, i) << s;
			}
		}

		if((reg & 7) == 5 && disp_size == 0)
		{
			disp_size = 2;
		}
		else
		{
			if(execute)
				emu->parser->address_offset += x86_register_get64(emu, reg);
		}

		strcpy(format, "[%s:");
		if(!((reg & 7) == 5 && disp_size == 0))
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s+", x86_register_name64(prs, reg));
		}
		if(i != 4)
		{
			snprintf(format + strlen(format), sizeof(format) - strlen(format), "%s*%d+", x86_register_name64(prs, i), 1 << s);
		}
		strcat(format, "%X]");
	}
	else if((reg & 7) == 5 && disp_size == 0)
	{
		prs->ip_relative = true;
		disp_size = 2;
		strcpy(format, "[%s:rip+%X]");
	}
	else
	{
		if(execute)
			emu->parser->address_offset = x86_register_get64(emu, reg);
		snprintf(format, sizeof format, "[%%s:%s+%%X]", x86_register_name64(prs, reg));
	}

	switch(disp_size)
	{
	case 1:
		prs->address_offset += displacement = (int8_t)x86_fetch8(prs, emu);
		break;
	case 2:
		prs->address_offset += displacement = x86_fetch32(prs, emu);
		break;
	}

	if(prs->segment == NONE)
	{
		prs->segment = X86_R_DS;
	}

	sprintf(prs->address_text, format, x86_segment_name(prs, prs->segment), (int32_t)displacement);
}

static inline void x86_parse_modrm(x86_parser_t * prs, x86_state_t * emu, bool execute)
{
	if(prs->address_size == X86_SIZE_WORD)
	{
		x86_parse_modrm16(prs, emu, execute);
	}
	else if(prs->address_size == X86_SIZE_QUAD)
	{
		x86_parse_modrm64(prs, emu, execute);
	}
	else if(prs->code_size == X86_SIZE_QUAD)
	{
		x86_parse_modrm64_32(prs, emu, execute);
	}
	else
	{
		x86_parse_modrm32(prs, emu, execute);
	}
}

static inline void x86_calculate_operand_address(x86_state_t * emu)
{
	if(emu->parser->ip_relative)
		emu->parser->address_offset += emu->xip;
	switch(emu->parser->address_size)
	{
	case SIZE_16BIT:
		emu->parser->address_offset &= 0xFFFF;
		break;
	case SIZE_32BIT:
		emu->parser->address_offset &= 0xFFFFFFFF;
		break;
	default:
		break;
	}
}

#define X86_CHECK_O(emu) ((emu)->of != 0)
#define X86_CHECK_NO(emu) (!X86_CHECK_O(emu))
#define X86_CHECK_C(emu) ((emu)->cf != 0)
#define X86_CHECK_NC(emu) (!X86_CHECK_C(emu))
#define X86_CHECK_B(emu) X86_CHECK_C(emu)
#define X86_CHECK_NB(emu) (!X86_CHECK_B(emu))
#define X86_CHECK_Z(emu) ((emu)->zf != 0)
#define X86_CHECK_NZ(emu) (!X86_CHECK_Z(emu))
#define X86_CHECK_BE(emu) (X86_CHECK_B(emu)||X86_CHECK_Z(emu))
#define X86_CHECK_NBE(emu) (!X86_CHECK_BE(emu))
#define X86_CHECK_S(emu) ((emu)->sf != 0)
#define X86_CHECK_NS(emu) (!X86_CHECK_S(emu))
#define X86_CHECK_P(emu) ((emu)->pf != 0)
#define X86_CHECK_NP(emu) (!X86_CHECK_P(emu))
#define X86_CHECK_L(emu) ((X86_CHECK_S(emu) != 0) != (X86_CHECK_O(emu) != 0))
#define X86_CHECK_NL(emu) (!X86_CHECK_L(emu))
#define X86_CHECK_LE(emu) (X86_CHECK_L(emu)||X86_CHECK_Z(emu))
#define X86_CHECK_NLE(emu) (!X86_CHECK_LE(emu))

static inline bool x86_rep_condition(x86_state_t * emu)
{
	switch(emu->parser->rep_prefix)
	{
	default:
		return true;
	case X86_PREF_REPZ:
		return X86_CHECK_Z(emu);
	case X86_PREF_REPNZ:
		return X86_CHECK_NZ(emu);
	case X86_PREF_REPC: // NEC specific
		return X86_CHECK_C(emu);
	case X86_PREF_REPNC: // NEC specific
		return X86_CHECK_NC(emu);
	}
}

static inline void x86_undefined_instruction(x86_state_t * emu)
{
	if(emu->cpu_type == X86_CPU_186 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type >= X86_CPU_286)
	{
		x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0);
	}
	else
	{
		emu->emulation_result = X86_RESULT(X86_RESULT_UNDEFINED, 0);
	}
}

#define _int8 int8_t
#define _int16 int16_t
#define _int32 int32_t
#define _int64 int64_t
#define _int128 int128_t

#define _uint8 uint8_t
#define _uint16 uint16_t
#define _uint32 uint32_t
#define _uint64 uint64_t
#define _uint128 uint128_t

#define _lowest8  (-0x80)
#define _lowest16 (-0x8000)
#define _lowest32 (-0x80000000)
#define _lowest64 (-0x8000000000000000)

#define _parity(x) (x86_flags_p[(x) & 0xFF] >> 2)

#define _rol8(x, y)  (((x) << (y)) | ((x) >> (8  - (y))))
#define _rol16(x, y) (((x) << (y)) | ((x) >> (16 - (y))))
#define _rol32(x, y) (((x) << (y)) | ((x) >> (32 - (y))))
#define _rol64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))

#define _ror8(x, y)  (((x) >> (y)) | ((x) << (8  - (y))))
#define _ror16(x, y) (((x) >> (y)) | ((x) << (16 - (y))))
#define _ror32(x, y) (((x) >> (y)) | ((x) << (32 - (y))))
#define _ror64(x, y) (((x) >> (y)) | ((x) << (64 - (y))))

#define _rcl8(x, y)  (((x) << (y)) | ((uoff_t)emu->cf << ((y) - 1)) | ((x) >> (8  + 1 - (y))))
#define _rcl16(x, y) (((x) << (y)) | ((uoff_t)emu->cf << ((y) - 1)) | ((x) >> (16 + 1 - (y))))
#define _rcl32(x, y) (((x) << (y)) | ((uoff_t)emu->cf << ((y) - 1)) | ((x) >> (32 + 1 - (y))))
#define _rcl64(x, y) (((x) << (y)) | ((uoff_t)emu->cf << ((y) - 1)) | ((x) >> (64 + 1 - (y))))

#define _rcr8(x, y)  (((x) >> (y)) | ((uoff_t)emu->cf << (8  - (y))) | ((x) << (8  + 1 - (y))))
#define _rcr16(x, y) (((x) >> (y)) | ((uoff_t)emu->cf << (16 - (y))) | ((x) << (16 + 1 - (y))))
#define _rcr32(x, y) (((x) >> (y)) | ((uoff_t)emu->cf << (32 - (y))) | ((x) << (32 + 1 - (y))))
#define _rcr64(x, y) (((x) >> (y)) | ((uoff_t)emu->cf << (64 - (y))) | ((x) << (64 + 1 - (y))))

#define _zero8(x)  ((_uint8) (x) == 0)
#define _zero16(x) ((_uint16)(x) == 0)
#define _zero32(x) ((_uint32)(x) == 0)
#define _zero64(x) ((_uint64)(x) == 0)

#define _sign8(x)  (((x) & 0x80) != 0)
#define _sign16(x) (((x) & 0x8000) != 0)
#define _sign32(x) (((x) & 0x80000000) != 0)
#define _sign64(x) (((x) & 0x8000000000000000) != 0)

#define _add_carry8(x, y, z)  (((((x) & (y)) | ((x) & ~(z)) | ((y) & ~(z))) & 0x80) != 0)
#define _add_carry16(x, y, z) (((((x) & (y)) | ((x) & ~(z)) | ((y) & ~(z))) & 0x8000) != 0)
#define _add_carry32(x, y, z) (((((x) & (y)) | ((x) & ~(z)) | ((y) & ~(z))) & 0x80000000) != 0)
#define _add_carry64(x, y, z) (((((x) & (y)) | ((x) & ~(z)) | ((y) & ~(z))) & 0x8000000000000000) != 0)

#define _sub_carry8(x, y, z)  (((((x) & ~(y)) | ((x) & ~(z)) | (~(y) & ~(z))) & 0x80) == 0)
#define _sub_carry16(x, y, z) (((((x) & ~(y)) | ((x) & ~(z)) | (~(y) & ~(z))) & 0x8000) == 0)
#define _sub_carry32(x, y, z) (((((x) & ~(y)) | ((x) & ~(z)) | (~(y) & ~(z))) & 0x80000000) == 0)
#define _sub_carry64(x, y, z) (((((x) & ~(y)) | ((x) & ~(z)) | (~(y) & ~(z))) & 0x8000000000000000) == 0)

#define _add_auxiliary(x, y, z) ((((x) ^ (y) ^ (z)) & 0x10) != 0)
#define _sub_auxiliary(x, y, z) ((((x) ^ (y) ^ (z)) & 0x10) != 0)

#define _add_auxiliaryh(x, y, z) ((((x) ^ (y) ^ (z)) & 0x1000) != 0)
#define _sub_auxiliaryh(x, y, z) ((((x) ^ (y) ^ (z)) & 0x1000) == 0)

#define _overflow8(x, z)  ((((x) ^ (z)) >> (8  - 1)) & 1)
#define _overflow16(x, z) ((((x) ^ (z)) >> (16 - 1)) & 1)
#define _overflow32(x, z) ((((x) ^ (z)) >> (32 - 1)) & 1)
#define _overflow64(x, z) ((((x) ^ (z)) >> (64 - 1)) & 1)

#define _add_overflow8(x, y, z)  (((((x) & (y) & ~(z)) | (~(x) & ~(y) & (z))) & 0x80) != 0)
#define _add_overflow16(x, y, z) (((((x) & (y) & ~(z)) | (~(x) & ~(y) & (z))) & 0x8000) != 0)
#define _add_overflow32(x, y, z) (((((x) & (y) & ~(z)) | (~(x) & ~(y) & (z))) & 0x80000000) != 0)
#define _add_overflow64(x, y, z) (((((x) & (y) & ~(z)) | (~(x) & ~(y) & (z))) & 0x8000000000000000) != 0)

#define _sub_overflow8(x, y, z)  (((((x) & ~(y) & ~(z)) | (~(x) & (y) & (z))) & 0x80) != 0)
#define _sub_overflow16(x, y, z) (((((x) & ~(y) & ~(z)) | (~(x) & (y) & (z))) & 0x8000) != 0)
#define _sub_overflow32(x, y, z) (((((x) & ~(y) & ~(z)) | (~(x) & (y) & (z))) & 0x80000000) != 0)
#define _sub_overflow64(x, y, z) (((((x) & ~(y) & ~(z)) | (~(x) & (y) & (z))) & 0x8000000000000000) != 0)

#define DEBUG(...) do { if(disassemble) debug_printf((prs)->debug_output, __VA_ARGS__); } while(0) // TODO: better type

// Called when the instruction is not defined, either triggers an interrupt or returns without any action
#define UNDEFINED() \
	do \
	{ \
		DEBUG("ud"); \
		if(execute) \
		{ \
			x86_undefined_instruction(emu); \
		} \
		return; \
	} while(0)

// Called when the instruction is not defined, either triggers an interrupt or returns without any action
#define X87_UNDEFINED() \
	do \
	{ \
		DEBUG("ud"); \
		if(execute) \
		{ \
			if(prs->fpu_type == X87_FPU_INTEGRATED) \
			{ \
				x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0); \
			} \
		} \
		return; \
	} while(0)

// Should be called before executing an instruction requiring system mode (current privilege level 0)
#define PRIVILEGED() \
	do \
	{ \
		if(execute && x86_get_cpl(emu) != 0) \
		{ \
			x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, 0); \
		} \
	} while(0)

// Should be called before executing an instruction that may not have a LOCK prefix
#define NO_LOCK() \
	do \
	{ \
		if(emu->cpu_type == X86_CPU_186 || emu->cpu_type >= X86_CPU_286) \
		{ \
			if(emu->parser->lock_prefix) \
				x86_undefined_instruction(emu); \
		} \
	} while(0)

// Should be called before I/O instructions, only relevant for v25/v55
#define IO_PRIVILEGED() \
	do \
	{ \
		if(execute && emu->ibrk_ == 0) \
			x86_trigger_interrupt(emu, X86_EXC_IO | X86_EXC_FAULT, 0); \
	} while(0)

#define REGFLDVAL(prs) (((prs)->modrm_byte >> 3) & 7)
#define REGFLD(prs) (REGFLDVAL(prs) | ((prs)->rex_r))
#define REGFLDLOCK(prs) (REGFLDVAL(prs) | ((prs)->rex_r) | ((prs)->lock_prefix ? 8 : 0))
#define _reg REGFLD(prs)

#define MEMFLDVAL(prs) ((prs)->modrm_byte & 7)
#define MEMFLD(prs) (MEMFLDVAL(prs) | ((prs)->rex_b))
#define _mem MEMFLD(prs)

#define REGNUM(prs, value) ((value) | (prs)->rex_b)

#define _seg prs->segment
#define _off prs->address_offset

#define _dst_seg prs->destination_segment
#define _src_seg prs->source_segment
#define _src_seg2 prs->source_segment2
#define _src_seg3 prs->source_segment3

#define _rep() x86_rep_condition(emu)

#define _read8(seg, off) x86_memory_segmented_read8(emu, seg, off)
#define _read16(seg, off) x86_memory_segmented_read16(emu, seg, off)
#define _read32(seg, off) x86_memory_segmented_read32(emu, seg, off)
#define _read64(seg, off) x86_memory_segmented_read64(emu, seg, off)

#define _x87_read16(seg, off) x87_memory_segmented_read16(emu, seg, off, off)
#define _x87_read32(seg, off) x87_memory_segmented_read32(emu, seg, off, off)
#define _x87_read64(seg, off) x87_memory_segmented_read64(emu, seg, off, off)
#define _x87_read32fp(seg, off) x87_convert32_to_float(_x87_read32(seg, off))
#define _x87_read64fp(seg, off) x87_convert64_to_float(_x87_read64(seg, off))
#define _x87_read80fp(seg, off) x87_memory_segmented_read80fp(emu, seg, off, off)

#define _write8(seg, off, val) x86_memory_segmented_write8(emu, seg, off, val)
#define _write16(seg, off, val) x86_memory_segmented_write16(emu, seg, off, val)
#define _write32(seg, off, val) x86_memory_segmented_write32(emu, seg, off, val)
#define _write64(seg, off, val) x86_memory_segmented_write64(emu, seg, off, val)

#define _x87_write16(seg, off, val) x87_memory_segmented_write16(emu, seg, off, off, val)
#define _x87_write32(seg, off, val) x87_memory_segmented_write32(emu, seg, off, off, val)
#define _x87_write64(seg, off, val) x87_memory_segmented_write64(emu, seg, off, off, val)
#define _x87_write32fp(seg, off, val) _x87_write32(seg, off, x87_convert32_from_float(val))
#define _x87_write64fp(seg, off, val) _x87_write64(seg, off, x87_convert64_from_float(val))
#define _x87_write80fp(seg, off, val) x87_memory_segmented_write80fp(emu, seg, off, off, val)

#define _input8(port) x86_input8(emu, port)
#define _input16(port) x86_input16(emu, port)
#define _input32(port) x86_input32(emu, port)

#define _output8(port, value) x86_output8(emu, port, value)
#define _output16(port, value) x86_output16(emu, port, value)
#define _output32(port, value) x86_output32(emu, port, value)

/* 8086 undefined */
#define _push8(val)  x86_push8(emu, val)
#define _push16(val) x86_push16(emu, val)
#define _push32(val) x86_push32(emu, val)
#define _push64(val) x86_push64(emu, val)

#define _pop16() x86_pop16(emu)
#define _pop32() x86_pop32(emu)
#define _pop64() x86_pop64(emu)

#define _crget32(x) x86_control_register_get32(emu, x)
#define _crget64(x) x86_control_register_get64(emu, x)
#define _crset32(x,y) x86_control_register_set32(emu, x, y)
#define _crset64(x,y) x86_control_register_set64(emu, x, y)

#define _drget32(x) x86_debug_register_get32(emu, x)
#define _drget64(x) x86_debug_register_get64(emu, x)
#define _drset32(x,y) x86_debug_register_set32(emu, x, y)
#define _drset64(x,y) x86_debug_register_set64(emu, x, y)

#define _trget32(x) x86_test_register_get32(emu, x)
#define _trset32(x,y) x86_test_register_set32(emu, x, y)

#define X80_CHECK_C(emu)  (((emu)->bank[0].af & X86_FL_CF) != 0)
#define X80_CHECK_NC(emu) (!X80_CHECK_C(emu))
#define X80_CHECK_Z(emu)  (((emu)->bank[0].af & X86_FL_ZF) != 0)
#define X80_CHECK_NZ(emu) (!X80_CHECK_Z(emu))
#define X80_CHECK_M(emu)  (((emu)->bank[0].af & X86_FL_SF) != 0)
#define X80_CHECK_P(emu)  (!X80_CHECK_M(emu))
#define X80_CHECK_PE(emu) (((emu)->bank[0].af & X86_FL_PF) != 0)
#define X80_CHECK_PO(emu) (!X80_CHECK_PE(emu))

#define _jmpf(seg, off) x86_jump_far(emu, seg, off)

/* 8086 undefined */
#define _callf8(seg, off) \
	do { \
		x86_push8(emu, emu->sr[X86_R_CS].selector); \
		x86_push8(emu, emu->xip); \
		x86_segment_load_real_mode(emu, X86_R_CS, seg); \
		emu->xip = off; \
	} while(0)
#define _callf16(seg, off) x86_call_far16(emu, seg, off)
#define _callf32(seg, off) x86_call_far32(emu, seg, off)
#define _callf64(seg, off) x86_call_far64(emu, seg, off)

#define _iret16() x86_return_interrupt16(emu)
#define _iret32() x86_return_interrupt32(emu)
#define _iret64() x86_return_interrupt64(emu)

#define _retf16(bytes) x86_return_far16(emu, bytes)
#define _retf32(bytes) x86_return_far32(emu, bytes)
#define _retf64(bytes) x86_return_far64(emu, bytes)

#define _int80em(num, param) do { emu->pc = old_pc; x86_enter_interrupt(emu86, num, param); } while(0)

#define _read80b(off) x80_memory_read8(emu, (off))
#define _write80b(off, val) x80_memory_write8(emu, (off), (val))
#define _pop80() x80_pop16(emu)
#define _push80(val) x80_push16(emu, (val))

#define _x87_int() \
	do \
	{ \
		if((emu->x87.sw & X87_SW_ES) != 0) \
		{ \
			x87_trigger_interrupt(emu); \
		} \
	} while(0)

#define _x87_busy() \
	do \
	{ \
		if(sync) \
		{ \
			if(emu->x87.fpu_type == X87_FPU_8087) \
			{ \
				if((emu->x87.sw & X87_SW_B) != 0) \
					return; \
			} \
			else if(X87_FPU_287 <= emu->x87.fpu_type && emu->x87.fpu_type < X87_FPU_INTEGRATED) \
			{ \
				if((emu->x87.sw & X87_SW_B) != 0) \
				{ \
					emu->xip = emu->old_xip; \
					return; \
				} \
			} \
			emu->x87.next_fop = fop; \
			emu->x87.next_fcs = fcs; \
			emu->x87.next_fip = fip; \
			emu->x87.next_fds = fds; \
			emu->x87.next_fdp = fdp; \
			emu->x87.segment = _seg; \
			emu->x87.offset = _off; \
			if(emu->x87.fpu_type != X87_FPU_INTEGRATED) \
			{ \
				emu->x87.sw |= X87_SW_B; \
				return; \
			} \
		} \
		else \
		{ \
			emu->x87.sw |= X87_SW_B; \
		} \
	} while(0)

// SIMD instructions

#define _mmx() \
	do { \
		if((emu->cr[0] & X86_CR0_EM) != 0) \
			x86_trigger_interrupt(emu, X86_EXC_UD | X86_EXC_FAULT, 0); \
		_x87_int(); \
		x87_set_sw_top(emu, 0); \
		emu->x87.tw = 0x0000; \
	} while(0)

#define _FOR(__variable, __start, __end) for(uint8_t __variable = (__start); __variable <= (__end); __variable ++)

static inline int16_t _extsbw(int8_t value)
{
	return value;
}

static inline int32_t _extswl(int16_t value)
{
	return value;
}

static inline int8_t _satswsb(int16_t value)
{
	if(value < -0x80)
		return -0x80;
	else if(value > 0x7F)
		return 0x7F;
	else
		return value;
}

static inline uint8_t _satswub(int16_t value)
{
	if(value < 0x00)
		return 0x00;
	else if(value > 0xFF)
		return 0xFF;
	else
		return value;
}

static inline uint8_t _satuwub(uint16_t value)
{
	if(value > 0xFF)
		return 0xFF;
	else
		return value;
}

static inline int16_t _satslsw(int32_t value)
{
	if(value < -0x8000)
		return -0x8000;
	else if(value > 0x7FFF)
		return 0x7FFF;
	else
		return value;
}

static inline uint16_t _satuluw(uint32_t value)
{
	if(value > 0xFFFF)
		return 0xFFFF;
	else
		return value;
}

/*
	Parses an x87 instructions
	prs - The parser state, should reference emu->parser if emu is not NULL
	emu - The emulator state, only needed if execute == true
	sync - true if called from x86_parse, false if called separately
	fop, fcs, fip, fds, fdp - Values for the exception pointers, if those need to be set
	segment_number, segment_offset - Parsed operand
	disassemble - true to fill debug output buffer with the disassembled instruction
	execute - true to execute instruction, false to only disassemble
*/
static inline void x87_parse(x86_parser_t * prs, x86_state_t * emu, bool sync, uint16_t fop, uint16_t fcs, uaddr_t fip, uint16_t fds, uaddr_t fdp, x86_segnum_t segment_number, uoff_t segment_offset, bool disassemble, bool execute);

#include "x86.gen.c"

#undef DEBUG

#define DEBUG(...) do { if(disassemble) debug_printf((emu) ? (emu)->parser->debug_output : (prs)->debug_output, __VA_ARGS__); } while(0) // TODO: better type

static inline void x89_parse(x89_parser_t * prs, x86_state_t * emu, unsigned channel_number, bool disassemble, bool execute)
{
	// used for MOV M, M
	int data_size = 0;
	uint16_t data = 0;
	const char * src_op_format = NULL;
	int src_base_field = 0;
	uint8_t src_disp;

	uint16_t ins;

restart:
	ins = x89_fetch16(prs, emu, channel_number);

	if(data_size != 0 && ((ins >> 10) != 0b110011 || (_OPSIZE == 0 ? data_size != 1 : data_size != 2)))
	{
		DEBUG("ud");
		// not second part of MOV M, M, abandon instruction (to avoid infinite loops in the emulator)
		prs->current_position -= 2;
		if(execute)
			emu->x89.channel[channel_number].r[X89_R_TP].address -= 2;
		return;
	}

	x89_address_t addr;
	const char * op_format;
	uint8_t disp;
	switch((ins >> 1) & 3)
	{
	case 0:
		op_format = "[%s]";
		if(execute)
		{
			addr = x89_base_register_get(emu, channel_number, (ins >> 8) & 3);
		}
		break;
	case 1:
		op_format = "[%s].%X";
		disp = x89_fetch8(prs, emu, channel_number);
		if(execute)
		{
			addr = x89_base_register_get(emu, channel_number, (ins >> 8) & 3);
			addr.address += disp;
		}
		break;
	case 2:
		op_format = "[%s+ix]";
		if(execute)
		{
			addr = x89_base_register_get(emu, channel_number, (ins >> 8) & 3);
			addr.address += x89_register_get16(emu, channel_number, X89_R_IX);
		}
		break;
	case 3:
		op_format = "[%s+ix+]";
		if(execute)
		{
			uint16_t ix;
			addr = x89_base_register_get(emu, channel_number, (ins >> 8) & 3);
			ix = x89_register_get16(emu, channel_number, X89_R_IX);
			addr.address += ix;
			ix += (ins & 1) ? 2 : 1;
			x89_register_set16(emu, channel_number, X89_R_IX, ix);
		}
		break;
	}

	int16_t literal;
	int16_t segment = 0;
	switch(_IMSIZE)
	{
	case 0:
		literal = 0;
		break;
	case 1:
		literal = (int8_t)x89_fetch8(prs, emu, channel_number);
		break;
	case 2:
		literal = x89_fetch16(prs, emu, channel_number);
		if((ins >> 10) == 0b000010)
		{
			// LPDI
			segment = x89_fetch16(prs, emu, channel_number);
		}
		break;
	case 3:
		segment = x89_fetch16(prs, emu, channel_number); // actually another literal
		literal = x89_fetch16(prs, emu, channel_number);
		break;
	}

	switch(ins >> 10)
	{
	case 0b000000:
		switch((ins >> 5) & 7)
		{
		case 0b000:
			// NOP
			DEBUG("nop\n");
			break;
		case 0b010:
			// SINTR
			DEBUG("sintr\n");
			if(!execute)
				return;
			if((emu->x89.channel[channel_number].psw & X89_PSW_IC) != 0)
			{
				emu->x89.channel[channel_number].psw |= X89_PSW_IS;
			}
			break;
		case 0b011:
			// XFER
			DEBUG("xfer\n");
			if(!execute)
				return;
			emu->x89.channel[channel_number].start_transfer = true;
			return; // wait 1 instruction before starting
		case 0b100:
		case 0b101:
		case 0b110:
		case 0b111:
			// WID
			DEBUG("wid\t%d, %d\n", ins & 0x0040 ? 16 : 8, ins & 0x0020 ? 16 : 8);
			if(!execute)
				return;
			if((ins & 0x2000) != 0)
			{
				emu->x89.channel[channel_number].psw |= X89_PSW_D;
			}
			else
			{
				emu->x89.channel[channel_number].psw &= ~X89_PSW_D;
			}
			if((ins & 0x4000) != 0)
			{
				emu->x89.channel[channel_number].psw |= X89_PSW_S;
			}
			else
			{
				emu->x89.channel[channel_number].psw &= ~X89_PSW_S;
			}
			break;
		default:
			DEBUG("ud\n");
			break;
		}
		break;
	case 0b000010:
		// LPDI P, I
		DEBUG("ldpi\t%s, %X:%X\n", x89_register_name[_REGFLD], segment & 0xFFFF, literal & 0xFFFF);
		if(!execute)
			return;
		emu->x89.channel[channel_number].r[_REGFLD].address = (segment << 4) + (literal & 0xFFFF);
		emu->x89.channel[channel_number].r[_REGFLD].tag = 0;
		break;
	case 0b001000:
		// ADDI R, I & JMP
		if(_REGFLD == X89_R_TP)
		{
			if(_IMSIZE == 1)
			{
				DEBUG("jmp\t%X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
			}
			else
			{
				DEBUG("ljmp\t%X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
			}
		}
		else
		{
			if(_OPSIZE == 0)
			{
				DEBUG("addbi\t%s, %X\n", x89_register_name[_REGFLD], (int8_t)literal);
			}
			else
			{
				DEBUG("addi\t%s, %X\n", x89_register_name[_REGFLD], literal);
			}
		}
		if(!execute)
			return;
		if(_OPSIZE == 0)
		{
			x89_register_set32(emu, channel_number, _REGFLD,
				x89_register_get32(emu, channel_number, _REGFLD) + (int8_t)literal);
		}
		else
		{
			x89_register_set32(emu, channel_number, _REGFLD,
				x89_register_get32(emu, channel_number, _REGFLD) + literal);
		}
		break;
	case 0b001001:
		// ORI R, I
		if(_OPSIZE == 0)
		{
			DEBUG("orbi\t%s, %X\n", x89_register_name[_REGFLD], (int8_t)literal);
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) | (int8_t)literal);
		}
		else
		{
			DEBUG("ori\t%s, %X\n", x89_register_name[_REGFLD], literal);
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) | literal);
		}
		break;
	case 0b001010:
		// ANDI R, I
		if(_OPSIZE == 0)
		{
			DEBUG("andbi\t%s, %X\n", x89_register_name[_REGFLD], (int8_t)literal);
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) & (int8_t)literal);
		}
		else
		{
			DEBUG("andi\t%s, %X\n", x89_register_name[_REGFLD], literal);
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) & literal);
		}
		break;
	case 0b001011:
		// NOT R
		DEBUG("not\t%s\n", x89_register_name[_REGFLD]);
		if(!execute)
			return;
		x89_register_set16(emu, channel_number, _REGFLD, ~x89_register_get16(emu, channel_number, _REGFLD));
		break;
	case 0b001100:
		// MOVI R, I
		if(_OPSIZE == 0)
		{
			DEBUG("movbi\t%s, %X\n", x89_register_name[_REGFLD], (int8_t)literal);
			if(!execute)
				return;
			x89_register_set16_local(emu, channel_number, _REGFLD, (int8_t)literal);
		}
		else
		{
			DEBUG("movi\t%s, %X\n", x89_register_name[_REGFLD], literal);
			if(!execute)
				return;
			x89_register_set16_local(emu, channel_number, _REGFLD, literal);
		}
		break;
	case 0b001110:
		// INC R
		DEBUG("inc\t%s\n", x89_register_name[_REGFLD]);
		if(!execute)
			return;
		x89_register_set32(emu, channel_number, _REGFLD,
			x89_register_get32(emu, channel_number, _REGFLD) + 1);
		break;
	case 0b001111:
		// DEC R
		DEBUG("dec\t%s\n", x89_register_name[_REGFLD]);
		if(!execute)
			return;
		x89_register_set32(emu, channel_number, _REGFLD,
			x89_register_get32(emu, channel_number, _REGFLD) - 1);
		break;
	case 0b010000:
		// JNZ R
		if(_IMSIZE == 1)
		{
			DEBUG("jnz\t%s, %X\n", x89_register_name[_REGFLD], emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
		}
		else
		{
			DEBUG("ljnz\t%s, %X\n", x89_register_name[_REGFLD], emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
		}

		if(!execute)
			return;
		if(x89_register_get16(emu, channel_number, _REGFLD) != 0)
			emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		break;
	case 0b010001:
		// JZ R
		if(_IMSIZE == 1)
		{
			DEBUG("jz\t%s, %X\n", x89_register_name[_REGFLD], emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
		}
		else
		{
			DEBUG("ljz\t%s, %X\n", x89_register_name[_REGFLD], emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
		}

		if(!execute)
			return;
		if(x89_register_get16(emu, channel_number, _REGFLD) == 0)
			emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		break;
	case 0b010010:
		// HLT
		DEBUG("hlt\n");
		if(!execute)
			return;
		x89_channel_busy_clear(emu, channel_number);
		break;
	case 0b010011:
		// MOVI M, I
		if(_OPSIZE == 0)
		{
			DEBUG("movbi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", (int8_t)literal);
			if(!execute)
				return;
			x89_write8(emu, addr, literal);
		}
		else
		{
			DEBUG("movi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", literal);
			if(!execute)
				return;
			x89_write16(emu, addr, literal);
		}
		break;
	case 0b100000:
		// MOV R, M
		if(_OPSIZE == 0)
		{
			DEBUG("movb\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16_local(emu, channel_number, _REGFLD, (int8_t)x89_read8(emu, addr));
		}
		else
		{
			DEBUG("mov\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16_local(emu, channel_number, _REGFLD, (int16_t)x89_read16(emu, addr));
		}
		break;
	case 0b100001:
		// MOV M, R
		if(_OPSIZE == 0)
		{
			DEBUG("movb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write8(emu, addr, x89_register_get16(emu, channel_number, _REGFLD));
		}
		else
		{
			DEBUG("mov\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write16(emu, addr, x89_register_get16(emu, channel_number, _REGFLD));
		}
		break;
	case 0b100010:
		// LPD P, M
		DEBUG("ldpi\t%s, ", x89_register_name[_REGFLD]);
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG("\n");
		if(!execute)
			return;
		x89_read_pair16(emu, addr, (uint16_t *)&literal, (uint16_t *)&segment);
		emu->x89.channel[channel_number].r[_REGFLD].address = (segment << 4) + (literal & 0xFFFF);
		emu->x89.channel[channel_number].r[_REGFLD].tag = 0;
		break;
	case 0b100011:
		// MOVP P, M
		DEBUG("movp\t%s, ", x89_register_name[_REGFLD]);
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG("\n");
		if(!execute)
			return;
		emu->x89.channel[channel_number].r[_REGFLD] = x89_read_address(emu, addr);
		break;
	case 0b100100:
		// MOV M, M
		if(_OPSIZE == 0)
		{
#if 0
			// delay printing until second part
			DEBUG("movb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
#endif
			src_op_format = op_format;
			src_base_field = _BASEFLD;
			src_disp = disp;
			data_size = 1;
			if(execute)
				data = x89_read8(emu, addr);
			goto restart;
		}
		else
		{
#if 0
			// delay printing until second part
			DEBUG("mov\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
#endif
			src_op_format = op_format;
			src_base_field = _BASEFLD;
			src_disp = disp;
			data_size = 2;
			if(execute)
				data = x89_read16(emu, addr);
			goto restart;
		}
		break;
	case 0b100101:
		// TSL M, I, L
		DEBUG("tsl\t");
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %X, %X\n", segment, emu->x89.channel[channel_number].r[X89_R_TP].address + literal);
		if(!execute)
			return;
		if(x89_read8(emu, addr) == 0)
		{
			x89_write8(emu, addr, segment);
		}
		else
		{
			emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		}
		break;
	case 0b100110:
		// MOVP M, P
		DEBUG("movp\t");
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %s\n", x89_register_name[_REGFLD]);
		if(!execute)
			return;
		x89_write_address(emu, addr, emu->x89.channel[channel_number].r[_REGFLD]);
		break;
	case 0b100111:
		// CALL M, P
		if(_IMSIZE == 1)
		{
			DEBUG("call\t");
		}
		else
		{
			DEBUG("lcall\t");
		}
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

		if(!execute)
			return;
		x89_write_address(emu, addr, emu->x89.channel[channel_number].r[X89_R_TP]);
		emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		break;
	case 0b101000:
		// ADD R, M
		if(_OPSIZE == 0)
		{
			DEBUG("addb\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set32(emu, channel_number, _REGFLD,
				x89_register_get32(emu, channel_number, _REGFLD) + (int8_t)x89_read8(emu, addr));
		}
		else
		{
			DEBUG("add\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set32(emu, channel_number, _REGFLD,
				x89_register_get32(emu, channel_number, _REGFLD) + (int16_t)x89_read16(emu, addr));
		}
		break;
	case 0b101001:
		// OR R, M
		if(_OPSIZE == 0)
		{
			DEBUG("orb\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) | (int8_t)x89_read8(emu, addr));
		}
		else
		{
			DEBUG("or\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) | x89_read16(emu, addr));
		}
		break;
	case 0b101010:
		// AND R, M
		if(_OPSIZE == 0)
		{
			DEBUG("andb\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) & (int8_t)x89_read8(emu, addr));
		}
		else
		{
			DEBUG("and\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD,
				x89_register_get16(emu, channel_number, _REGFLD) & x89_read16(emu, addr));
		}
		break;
	case 0b101011:
		// NOT R, M
		if(_OPSIZE == 0)
		{
			DEBUG("notb\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD, ~(int8_t)x89_read8(emu, addr));
		}
		else
		{
			DEBUG("not\t%s, ", x89_register_name[_REGFLD]);
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_register_set16(emu, channel_number, _REGFLD, ~x89_read16(emu, addr));
		}
		break;
	case 0b101100:
		// JMCE
		if(_IMSIZE == 1)
		{
			DEBUG("jmce\t");
		}
		else
		{
			DEBUG("ljmce\t");
		}
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

		if(!execute)
			return;
		{
			uint16_t mc = emu->x89.channel[channel_number].r[X89_R_MC].address;
			if(((x89_read8(emu, addr) ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) == 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		}
		break;
	case 0b101101:
		// JMCNE
		if(_IMSIZE == 1)
		{
			DEBUG("jmcne\t");
		}
		else
		{
			DEBUG("ljmcne\t");
		}
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

		if(!execute)
			return;
		{
			uint16_t mc = emu->x89.channel[channel_number].r[X89_R_MC].address;
			if(((x89_read8(emu, addr) ^ (mc & 0xFF)) & ((mc >> 8) & 0xFF)) != 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		}
		break;
	case 0b101110:
		// JNBT
		if(_IMSIZE == 1)
		{
			DEBUG("jnbt\t");
		}
		else
		{
			DEBUG("ljnbt\t");
		}
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %d, %X\n", _REGFLD, emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

		if(!execute)
			return;
		if((x89_read8(emu, addr) & (1 << _REGFLD)) == 0)
			emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		break;
	case 0b101111:
		// JBT
		if(_IMSIZE == 1)
		{
			DEBUG("jbt\t");
		}
		else
		{
			DEBUG("ljbt\t");
		}
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %d, %X\n", _REGFLD, emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

		if(!execute)
			return;
		if((x89_read8(emu, addr) & (1 << _REGFLD)) != 0)
			emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
		break;
	case 0b110000:
		// ADDI M, I
		if(_OPSIZE == 0)
		{
			DEBUG("addbi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", (int8_t)literal);
			if(!execute)
				return;
			x89_write8(emu, addr, x89_read8(emu, addr) + literal);
		}
		else
		{
			DEBUG("addi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", literal);
			if(!execute)
				return;
			x89_write16(emu, addr, x89_read16(emu, addr) + literal);
		}
		break;
	case 0b110001:
		// ORI M, I
		if(_OPSIZE == 0)
		{
			DEBUG("orbi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", (int8_t)literal);
			if(!execute)
				return;
			x89_write8(emu, addr, x89_read8(emu, addr) | literal);
		}
		else
		{
			DEBUG("ori\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", literal);
			if(!execute)
				return;
			x89_write16(emu, addr, x89_read16(emu, addr) | literal);
		}
		break;
	case 0b110010:
		// ANDI M, I
		if(_OPSIZE == 0)
		{
			DEBUG("andbi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", (int8_t)literal);
			if(!execute)
				return;
			x89_write8(emu, addr, x89_read8(emu, addr) & literal);
		}
		else
		{
			DEBUG("andi\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", literal);
			if(!execute)
				return;
			x89_write16(emu, addr, x89_read16(emu, addr) & literal);
		}
		break;
	case 0b110011:
		// MOV M, M (destination part)
		if(data_size == 0)
			break;
		if(_OPSIZE == 0)
		{
			DEBUG("movb\t");
			DEBUG(src_op_format, x89_base_register_name[src_base_field], src_disp);
			DEBUG(", ");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write8(emu, addr, data);
		}
		else
		{
			DEBUG("mov\t");
			DEBUG(src_op_format, x89_base_register_name[src_base_field], src_disp);
			DEBUG(", ");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write16(emu, addr, data);
		}
		break;
	case 0b110100:
		// ADD M, R
		if(_OPSIZE == 0)
		{
			DEBUG("addb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write8(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) + x89_read8(emu, addr));
		}
		else
		{
			DEBUG("add\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write16(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) + x89_read16(emu, addr));
		}
		break;
	case 0b110101:
		// OR M, R
		if(_OPSIZE == 0)
		{
			DEBUG("orb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write8(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) | x89_read8(emu, addr));
		}
		else
		{
			DEBUG("or\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write16(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) | x89_read16(emu, addr));
		}
		break;
	case 0b110110:
		// AND M, R
		if(_OPSIZE == 0)
		{
			DEBUG("andb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write8(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) & x89_read8(emu, addr));
		}
		else
		{
			DEBUG("and\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %s\n", x89_register_name[_REGFLD]);
			if(!execute)
				return;
			x89_write16(emu, addr,
				x89_register_get16(emu, channel_number, _REGFLD) & x89_read16(emu, addr));
		}
		break;
	case 0b110111:
		// NOT M
		if(_OPSIZE == 0)
		{
			DEBUG("notb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write8(emu, addr, ~x89_read8(emu, addr));
		}
		else
		{
			DEBUG("not\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write16(emu, addr, ~x89_read16(emu, addr));
		}
		break;
	case 0b111000:
		// JNZ M
		if(_OPSIZE == 0)
		{
			DEBUG("jnzb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

			if(!execute)
				return;
			if(x89_read8(emu, addr) != 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
			break;
		}
		else
		{
			DEBUG("jnz\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

			if(!execute)
				return;
			if(x89_read16(emu, addr) != 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
			break;
		}
		break;
	case 0b111001:
		// JZ M
		if(_OPSIZE == 0)
		{
			DEBUG("jzb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

			if(!execute)
				return;
			if(x89_read8(emu, addr) == 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
			break;
		}
		else
		{
			DEBUG("jz\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG(", %X\n", emu->x89.channel[channel_number].r[X89_R_TP].address + literal);

			if(!execute)
				return;
			if(x89_read16(emu, addr) == 0)
				emu->x89.channel[channel_number].r[X89_R_TP].address += literal;
			break;
		}
		break;
	case 0b111010:
		// INC M
		if(_OPSIZE == 0)
		{
			DEBUG("incb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write8(emu, addr,
				x89_read8(emu, addr) + 1);
		}
		else
		{
			DEBUG("inc\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write16(emu, addr,
				x89_read16(emu, addr) + 1);
		}
		break;
	case 0b111011:
		// DEC M
		if(_OPSIZE == 0)
		{
			DEBUG("decb\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write8(emu, addr,
				x89_read8(emu, addr) - 1);
		}
		else
		{
			DEBUG("dec\t");
			DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
			DEBUG("\n");
			if(!execute)
				return;
			x89_write16(emu, addr,
				x89_read16(emu, addr) - 1);
		}
		break;
	case 0b111101:
		// SETB
		DEBUG("setb\t");
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %d\n", _REGFLD);
		if(!execute)
			return;
		x89_write8(emu, addr,
			x89_read8(emu, addr) | (1 << _REGFLD));
		break;
	case 0b111110:
		// CLR
		DEBUG("clr\t");
		DEBUG(op_format, x89_base_register_name[_BASEFLD], disp);
		DEBUG(", %d\n", _REGFLD);
		if(!execute)
			return;
		x89_write8(emu, addr,
			x89_read8(emu, addr) & ~(1 << _REGFLD));
		break;
	default:
		DEBUG("ud\n");
		break;
	}
}


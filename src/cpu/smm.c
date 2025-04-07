
//// ICE (in-circuit emulator)/LOADALL (286, 386, 486) and system management mode (SMM) implementation

// Register caches

// Robert R. Collins: The Segment Register Cache
static inline void x86_descriptor_cache_read_286(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[6];
	x86_memory_read(emu, offset, 6, cache);
	segment->base = cache[0] | (cache[1] << 8) | ((uint32_t)cache[2] << 16);
	segment->access = cache[3] << 8;
	segment->limit = cache[4] | (cache[5] << 8);
}

static inline void x86_descriptor_cache_write_286(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[6];
	cache[0] = segment->base;
	cache[1] = segment->base >> 8;
	cache[2] = segment->base >> 16;
	cache[3] = segment->access >> 8;
	cache[4] = segment->limit;
	cache[5] = segment->limit >> 8;
	x86_memory_write(emu, offset, 6, cache);
}

// Robert R. Collins: The Segment Register Cache
static inline void x86_descriptor_cache_read_386(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[12];
	x86_memory_read(emu, offset, 12, cache);
	segment->access = (cache[2] << 8) | ((uint32_t)(cache[1] & 0x40) << 16);
	// the G flag is lost
	segment->base = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
	segment->limit = cache[8] | (cache[9] << 8) | ((uint32_t)cache[10] << 16) | ((uint32_t)cache[11] << 24);
}

static inline void x86_descriptor_cache_write_386(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[12];
	cache[0]  = 0;
	// the G flag is lost
	cache[1]  = (segment->access >> 16) & 0x40;
	cache[2]  = segment->access >> 8;
	cache[3]  = 0;
	cache[4]  = segment->base;
	cache[5]  = segment->base >> 8;
	cache[6]  = segment->base >> 16;
	cache[7]  = segment->base >> 24;
	cache[8]  = segment->limit;
	cache[9]  = segment->limit >> 8;
	cache[10] = segment->limit >> 16;
	cache[11] = segment->limit >> 24;
	x86_memory_write(emu, offset, 12, cache);
}

// Robert R. Collins: The Segment Register Cache
static inline void x86_descriptor_cache_read_p5(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[12];
	x86_memory_read(emu, offset, 12, cache);
	segment->limit = cache[0] | (cache[1] << 8) | ((uint32_t)cache[2] << 16) | ((uint32_t)cache[3] << 24);
	segment->base = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
	segment->access = (cache[8] << 8) | ((uint32_t)(cache[9] & 0x40) << 16);
}

static inline void x86_descriptor_cache_write_p5(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[12];
	cache[0]  = segment->limit;
	cache[1]  = segment->limit >> 8;
	cache[2]  = segment->limit >> 16;
	cache[3]  = segment->limit >> 24;
	cache[4]  = segment->base;
	cache[5]  = segment->base >> 8;
	cache[6]  = segment->base >> 16;
	cache[7]  = segment->base >> 24;
	// the G flag is lost
	cache[9]  = (segment->access >> 16) & 0x40;
	cache[8]  = segment->access >> 8;
	cache[10] = 0;
	cache[11] = 0;
	x86_memory_write(emu, offset, 12, cache);
}

// Robert R. Collins: The Segment Register Cache
static inline void x86_descriptor_cache_read_p6(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[12];
	x86_memory_read(emu, offset, 12, cache);
	segment->selector = cache[0] | (cache[1] << 8);
	segment->access = (cache[2] << 8) | ((uint32_t)(cache[3] & 0x40) << 16);
	segment->limit = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
	segment->base = cache[8] | (cache[9] << 8) | ((uint32_t)cache[10] << 16) | ((uint32_t)cache[11] << 24);
}

static inline void x86_descriptor_cache_write_p6(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[12];
	cache[0]  = segment->selector;
	cache[1]  = segment->selector >> 8;
	cache[2]  = segment->access >> 8;
	// the G flag is lost
	cache[3]  = (segment->access >> 16) & 0x40;
	cache[4]  = segment->limit;
	cache[5]  = segment->limit >> 8;
	cache[6]  = segment->limit >> 16;
	cache[7]  = segment->limit >> 24;
	cache[8]  = segment->base;
	cache[9]  = segment->base >> 8;
	cache[10] = segment->base >> 16;
	cache[11] = segment->base >> 24;
	x86_memory_write(emu, offset, 12, cache);
}

// sandpile.org
static inline void x86_descriptor_cache_read_p4(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[12];
	x86_memory_read(emu, offset, 12, cache);
	//if((cache[4] & 1) != 1)
	//	TODO: null selector
	segment->base = cache[0] | (cache[1] << 8) | ((uint32_t)cache[2] << 16) | ((uint32_t)cache[3] << 24);
	segment->access = (cache[5] << 7) | ((uint32_t)(cache[6] & 0xBF) << 15) | ((uint32_t)(cache[7] & 0x01) << 23);
	segment->limit = cache[8] | (cache[9] << 8) | ((uint32_t)cache[10] << 16) | ((uint32_t)cache[11] << 24);
}

static inline void x86_descriptor_cache_write_p4(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[12];
	cache[0] = segment->base;
	cache[1] = segment->base >> 8;
	cache[2] = segment->base >> 16;
	cache[3] = segment->base >> 24;
	cache[4] = (segment->selector & ~3) == 0 ? 0x01 : 0x00; // TODO: how to determine NULL selector
	cache[5] = segment->access >> 7;
	cache[6] = (segment->access >> 15) & 0xE1;
	if((segment->access & X86_DESC_G) == 0)
	{
		cache[6] |= (segment->limit >> 15) & 0x1E;
	}
	else
	{
		cache[6] |= (segment->limit >> 27) & 0x1E;
	}
	cache[7] = (segment->access >> 23) & 0x01;
	cache[8]  = segment->limit;
	cache[9]  = segment->limit >> 8;
	cache[10] = segment->limit >> 16;
	cache[11] = segment->limit >> 24;
	x86_memory_write(emu, offset, 12, cache);
}

// TODO: attribute word format unknown
static inline void x86_descriptor_cache_read_k5(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[12];
	x86_memory_read(emu, offset, 12, cache);
	segment->access = (cache[8] << 8) | ((uint32_t)(cache[9] & 0x0F) << 20); // TODO: guessing
	segment->limit = cache[0] | (cache[1] << 8) | ((uint32_t)(cache[2] & 0x0F) << 16);
	if((segment->access & X86_DESC_G))
	{
		segment->limit <<= 12;
		segment->limit |= 0xFFF;
	}
	segment->base = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
}

static inline void x86_descriptor_cache_read_k5_no_access(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[8];
	x86_memory_read(emu, offset, 8, cache);
	segment->limit = cache[0] | (cache[1] << 8) | ((uint32_t)(cache[2] & 0x0F) << 16);
	if((segment->access & X86_DESC_G))
	{
		segment->limit <<= 12;
		segment->limit |= 0xFFF;
	}
	segment->base = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
}

static inline void x86_descriptor_cache_write_k5(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[12];
	uint32_t limit = segment->limit;
	if((segment->access & X86_DESC_G))
	{
		limit >>= 12;
	}
	cache[0] = segment->limit;
	cache[1] = segment->limit >> 8;
	cache[2] = (segment->limit >> 16 & 0x0F);
	cache[3] = 0;
	cache[4] = segment->base;
	cache[5] = segment->base >> 8;
	cache[6] = segment->base >> 16;
	cache[7] = segment->base >> 24;
	cache[8] = segment->access >> 8;
	cache[9] = (segment->access >> 20) & 0x0F; // TODO: guessing
	cache[10] = 0;
	cache[11] = 0;
	x86_memory_write(emu, offset, 12, cache);
}

static inline void x86_descriptor_cache_write_k5_no_access(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[8];
	uint32_t limit = segment->limit;
	if((segment->access & X86_DESC_G))
	{
		limit >>= 12;
	}
	cache[0] = segment->limit;
	cache[1] = segment->limit >> 8;
	cache[2] = (segment->limit >> 16 & 0x0F);
	cache[3] = 0;
	cache[4] = segment->base;
	cache[5] = segment->base >> 8;
	cache[6] = segment->base >> 16;
	cache[7] = segment->base >> 24;
	x86_memory_write(emu, offset, 8, cache);
}

// TODO: attribute word format unknown
static inline void x86_descriptor_cache_read_64(x86_state_t * emu, uaddr_t offset, x86_segment_t * segment)
{
	uint8_t cache[16];
	x86_memory_read(emu, offset, 16, cache);
	segment->selector = cache[0] | (cache[1] << 8);
	segment->access = (cache[2] << 8) | ((uint32_t)(cache[3] & 0xF0) << 16); // TODO: guessing
	segment->limit = cache[4] | (cache[5] << 8) | ((uint32_t)cache[6] << 16) | ((uint32_t)cache[7] << 24);
	segment->base = cache[8] | (cache[9] << 8) | ((uint32_t)cache[10] << 16) | ((uint32_t)cache[11] << 24)
		| ((uint64_t)cache[12] << 32) | ((uint64_t)cache[13] << 40) | ((uint64_t)cache[14] << 48) | ((uint64_t)cache[15] << 56);
}

static inline void x86_descriptor_cache_write_64(x86_state_t * emu, uaddr_t offset, const x86_segment_t * segment)
{
	uint8_t cache[16];
	cache[0]  = segment->selector;
	cache[1]  = segment->selector >> 8;
	// TODO: guessing
	cache[2]  = segment->access >> 8;
	cache[3]  = (segment->access >> 16) & 0xF0;
	cache[4]  = segment->limit;
	cache[5]  = segment->limit >> 8;
	cache[6]  = segment->limit >> 16;
	cache[7]  = segment->limit >> 24;
	cache[8]  = segment->base;
	cache[9]  = segment->base >> 8;
	cache[10] = segment->base >> 16;
	cache[11] = segment->base >> 24;
	cache[12] = segment->base >> 32;
	cache[13] = segment->base >> 40;
	cache[14] = segment->base >> 48;
	cache[15] = segment->base >> 56;
	x86_memory_write(emu, offset, 16, cache);
}

//// ICE (STOREALL/LOADALL) for 286/386/486

static inline void x86_ice_storeall_286(x86_state_t * emu)
{
	emu->cpu_level = X86_LEVEL_ICE;
	// the F1 prefix can override the level for the following writes
	x86_memory_write16(emu, 0x00806, emu->cr[0] & 0xFFF0); // MSW
	x86_memory_write16(emu, 0x00818, (x86_flags_get16(emu) & 0x7FD5) | 0x0002);
	x86_memory_write16(emu, 0x0081A, emu->xip);
	x86_memory_write16(emu, 0x0081C, emu->sr[X86_R_LDTR].selector);
	x86_memory_write16(emu, 0x0081E, emu->sr[X86_R_DS].selector);
	x86_memory_write16(emu, 0x00820, emu->sr[X86_R_SS].selector);
	x86_memory_write16(emu, 0x00822, emu->sr[X86_R_CS].selector);
	x86_memory_write16(emu, 0x00824, emu->sr[X86_R_ES].selector);
	x86_memory_write16(emu, 0x00826, emu->gpr[X86_R_DI]);
	x86_memory_write16(emu, 0x00828, emu->gpr[X86_R_SI]);
	x86_memory_write16(emu, 0x0082A, emu->gpr[X86_R_BP]);
	x86_memory_write16(emu, 0x0082C, emu->gpr[X86_R_SP]);
	x86_memory_write16(emu, 0x0082E, emu->gpr[X86_R_BX]);
	x86_memory_write16(emu, 0x00830, emu->gpr[X86_R_DX]);
	x86_memory_write16(emu, 0x00832, emu->gpr[X86_R_CX]);
	x86_memory_write16(emu, 0x00834, emu->gpr[X86_R_AX]);
	x86_descriptor_cache_write_286(emu, 0x00836, &emu->sr[X86_R_ES]);
	x86_descriptor_cache_write_286(emu, 0x0083C, &emu->sr[X86_R_CS]);
	x86_descriptor_cache_write_286(emu, 0x00842, &emu->sr[X86_R_SS]);
	x86_descriptor_cache_write_286(emu, 0x00848, &emu->sr[X86_R_DS]);
	x86_descriptor_cache_write_286(emu, 0x0084E, &emu->sr[X86_R_GDTR]); // TODO: access byte should be 0xFF
	x86_descriptor_cache_write_286(emu, 0x00854, &emu->sr[X86_R_LDTR]); // TODO: access byte should be ORed with 0x7F
	x86_descriptor_cache_write_286(emu, 0x0085A, &emu->sr[X86_R_IDTR]); // TODO: access byte should be 0xFF
	x86_descriptor_cache_write_286(emu, 0x00860, &emu->sr[X86_R_TR]); // TODO: access byte should be 0xFF

	//x86_reset(false);
	for(int i = 0; i < 4; i++)
	{
		emu->sr[i].selector = 0;
		emu->sr[i].base = 0;
		emu->sr[i].limit = 0xFFFF;
		emu->sr[i].access = 0x9300;
	}

	emu->xip = 0xFFF0;
	emu->sr[X86_R_CS].selector = 0xF000;
	emu->sr[X86_R_CS].base = 0xFFFFF000;
	//emu->code_writable = true;

//	emu->sr[X86_R_GDTR].base = 0;
//	emu->sr[X86_R_GDTR].limit = 0xFFFF;
//	emu->sr[X86_R_GDTR].access = 0x8200;

	emu->sr[X86_R_IDTR].base = 0;
	emu->sr[X86_R_IDTR].limit = 0xFFFF;
	emu->sr[X86_R_IDTR].access = 0x8200;

	emu->sr[X86_R_LDTR].selector = 0;
//	emu->sr[X86_R_LDTR].base = 0;
//	emu->sr[X86_R_LDTR].limit = 0xFFFF;
//	emu->sr[X86_R_LDTR].access = 0x8200;

	emu->sr[X86_R_TR].selector = 0;
//	emu->sr[X86_R_TR].base = 0;
//	emu->sr[X86_R_TR].limit = 0xFFFF;
//	emu->sr[X86_R_TR].access = 0x8200;

	emu->cpl = 0;

	emu->cr[0] = 0xFFF0;

	/* FLAGS register */
	emu->cf = emu->pf = emu->af = emu->zf = emu->sf = emu->tf = emu->_if = emu->df = emu->of = 0;
	emu->md = 0;
	emu->iopl = emu->nt = 0;
}

static inline void x86_ice_loadall_286(x86_state_t * emu)
{
	emu->cr[0] = (emu->cr[0] & ~0xFFFE) | x86_memory_read16(emu, 0x00804); // MSW
	emu->sr[X86_R_TR].selector = x86_memory_read16(emu, 0x00816);
	x86_flags_set16(emu, (x86_memory_read16(emu, 0x00818) & 0x7FD5) | 0x0002);
	emu->xip = x86_memory_read16(emu, 0x0081A);
	emu->sr[X86_R_LDTR].selector = x86_memory_read16(emu, 0x0081C);
	emu->sr[X86_R_DS].selector = x86_memory_read16(emu, 0x0081E);
	emu->sr[X86_R_SS].selector = x86_memory_read16(emu, 0x00820);
	emu->sr[X86_R_CS].selector = x86_memory_read16(emu, 0x00822);
	emu->sr[X86_R_ES].selector = x86_memory_read16(emu, 0x00824);
	emu->gpr[X86_R_DI] = x86_memory_read16(emu, 0x00826);
	emu->gpr[X86_R_SI] = x86_memory_read16(emu, 0x00828);
	emu->gpr[X86_R_BP] = x86_memory_read16(emu, 0x0082A);
	emu->gpr[X86_R_SP] = x86_memory_read16(emu, 0x0082C);
	emu->gpr[X86_R_BX] = x86_memory_read16(emu, 0x0082E);
	emu->gpr[X86_R_DX] = x86_memory_read16(emu, 0x00830);
	emu->gpr[X86_R_CX] = x86_memory_read16(emu, 0x00832);
	emu->gpr[X86_R_AX] = x86_memory_read16(emu, 0x00834);
	x86_descriptor_cache_read_286(emu, 0x00836, &emu->sr[X86_R_ES]);
	x86_descriptor_cache_read_286(emu, 0x0083C, &emu->sr[X86_R_CS]);
	x86_descriptor_cache_read_286(emu, 0x00842, &emu->sr[X86_R_SS]);
	x86_descriptor_cache_read_286(emu, 0x00848, &emu->sr[X86_R_DS]);
	x86_descriptor_cache_read_286(emu, 0x0084E, &emu->sr[X86_R_GDTR]);
	x86_descriptor_cache_read_286(emu, 0x00854, &emu->sr[X86_R_LDTR]);
	x86_descriptor_cache_read_286(emu, 0x0085A, &emu->sr[X86_R_IDTR]);
	x86_descriptor_cache_read_286(emu, 0x00860, &emu->sr[X86_R_TR]);
	x86_set_cpl(emu, (emu->sr[X86_R_SS].access >> X86_DESC_DPL_SHIFT) & 3);
	emu->cpu_level = X86_LEVEL_USER;
}

static inline void x86_ice_storeall_386(x86_state_t * emu, uaddr_t offset)
{
	x86_memory_write32(emu, offset + 0x04, x86_flags_get32(emu));
	x86_memory_write32(emu, offset + 0x08, emu->xip);
	x86_memory_write32(emu, offset + 0x0C, emu->gpr[X86_R_DI]);
	x86_memory_write32(emu, offset + 0x10, emu->gpr[X86_R_SI]);
	x86_memory_write32(emu, offset + 0x14, emu->gpr[X86_R_BP]);
	x86_memory_write32(emu, offset + 0x18, emu->gpr[X86_R_SP]);
	x86_memory_write32(emu, offset + 0x1C, emu->gpr[X86_R_BX]);
	x86_memory_write32(emu, offset + 0x20, emu->gpr[X86_R_DX]);
	x86_memory_write32(emu, offset + 0x24, emu->gpr[X86_R_CX]);
	x86_memory_write32(emu, offset + 0x28, emu->gpr[X86_R_AX]);
	x86_memory_write32(emu, offset + 0x2C, emu->dr[6]);
	x86_memory_write32(emu, offset + 0x30, emu->dr[7]);
	x86_memory_write32(emu, offset + 0x34, emu->sr[X86_R_TR].selector);
	x86_memory_write32(emu, offset + 0x38, emu->sr[X86_R_LDTR].selector);
	x86_memory_write32(emu, offset + 0x3C, emu->sr[X86_R_GS].selector);
	x86_memory_write32(emu, offset + 0x40, emu->sr[X86_R_FS].selector);
	x86_memory_write32(emu, offset + 0x44, emu->sr[X86_R_DS].selector);
	x86_memory_write32(emu, offset + 0x48, emu->sr[X86_R_SS].selector);
	x86_memory_write32(emu, offset + 0x4C, emu->sr[X86_R_CS].selector);
	x86_memory_write32(emu, offset + 0x50, emu->sr[X86_R_ES].selector);
	x86_descriptor_cache_write_386(emu, offset + 0x54, &emu->sr[X86_R_TR]);
	x86_descriptor_cache_write_386(emu, offset + 0x60, &emu->sr[X86_R_IDTR]);
	x86_descriptor_cache_write_386(emu, offset + 0x6C, &emu->sr[X86_R_GDTR]);
	x86_descriptor_cache_write_386(emu, offset + 0x78, &emu->sr[X86_R_LDTR]);
	x86_descriptor_cache_write_386(emu, offset + 0x84, &emu->sr[X86_R_GS]);
	x86_descriptor_cache_write_386(emu, offset + 0x90, &emu->sr[X86_R_FS]);
	x86_descriptor_cache_write_386(emu, offset + 0x9C, &emu->sr[X86_R_DS]);
	x86_descriptor_cache_write_386(emu, offset + 0xA8, &emu->sr[X86_R_SS]);
	x86_descriptor_cache_write_386(emu, offset + 0xB4, &emu->sr[X86_R_CS]);
	x86_descriptor_cache_write_386(emu, offset + 0xC0, &emu->sr[X86_R_ES]);

	//x86_reset(false);
	for(int i = 0; i < 8; i++)
	{
		emu->gpr[i] = 0;
	}

	for(int i = 0; i < 6; i++)
	{
		emu->sr[i].selector = 0;
		emu->sr[i].base = 0;
		emu->sr[i].limit = 0xFFFFFFFF;
		emu->sr[i].access = 0x00409300;
	}

	emu->xip = 0xFFF0;
	emu->sr[X86_R_CS].selector = 0xF000;
	emu->sr[X86_R_CS].base = 0xFFFFF000;
	//emu->code_writable = true;

	emu->sr[X86_R_GDTR].base = 0;
	emu->sr[X86_R_GDTR].limit = 0xFFFF;
	emu->sr[X86_R_GDTR].access = 0x8200;

	emu->sr[X86_R_IDTR].base = 0;
	emu->sr[X86_R_IDTR].limit = 0xFFFF;
	emu->sr[X86_R_IDTR].access = 0x8200;

	emu->sr[X86_R_LDTR].base = 0;
	emu->sr[X86_R_LDTR].limit = 0xFFFF;
	emu->sr[X86_R_LDTR].access = 0x8200;

	emu->sr[X86_R_TR].base = 0;
	emu->sr[X86_R_TR].limit = 0xFFFF;
	emu->sr[X86_R_TR].access = 0x8200;

	emu->cpl = 0;

	emu->cr[0] = 0xFFF0;

	/*if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
	{
		emu->cr[0] = 0x0000001F;
	}*/ // TODO: does ICE exist on the Intel 80376?
	emu->cr[0] = 0x60000000;

	emu->cr[2] = 0;
	emu->cr[3] = 0;
	emu->cr[4] = 0;

	// TODO: what happens to the debug registers?
	emu->dr[0] = 0;
	emu->dr[1] = 0;
	emu->dr[2] = 0;
	emu->dr[3] = 0;
	emu->dr[6] = 0xFFFF0FF0;
	if(emu->cpu_type >= X86_CPU_586)
		emu->dr[7] = 0x00000400;
	else
		emu->dr[7] = 0x00000000;

	/* FLAGS register */
	emu->cf = emu->pf = emu->af = emu->zf = emu->sf = emu->tf = emu->_if = emu->df = emu->of = 0;
	emu->md = 0;
	emu->iopl = emu->nt = 0;
	emu->rf = emu->vm = 0;
	if(emu->cpu_type >= X86_CPU_486)
	{
		emu->ac = 0;
	}
}

static inline void x86_ice_loadall_386(x86_state_t * emu, uaddr_t offset)
{
	emu->cr[0] = x86_memory_read32(emu, offset + 0x00);
	x86_flags_set32(emu, x86_memory_read32(emu, offset + 0x04));
	emu->xip = x86_memory_read32(emu, offset + 0x08);
	emu->gpr[X86_R_DI] = x86_memory_read32(emu, offset + 0x0C);
	emu->gpr[X86_R_SI] = x86_memory_read32(emu, offset + 0x10);
	emu->gpr[X86_R_BP] = x86_memory_read32(emu, offset + 0x14);
	emu->gpr[X86_R_SP] = x86_memory_read32(emu, offset + 0x18);
	emu->gpr[X86_R_BX] = x86_memory_read32(emu, offset + 0x1C);
	emu->gpr[X86_R_DX] = x86_memory_read32(emu, offset + 0x20);
	emu->gpr[X86_R_CX] = x86_memory_read32(emu, offset + 0x24);
	emu->gpr[X86_R_AX] = x86_memory_read32(emu, offset + 0x28);
	emu->dr[6] = x86_memory_read32(emu, offset + 0x2C);
	emu->dr[7] = x86_memory_read32(emu, offset + 0x30);
	emu->sr[X86_R_TR].selector = x86_memory_read16(emu, offset + 0x34);
	emu->sr[X86_R_LDTR].selector = x86_memory_read16(emu, offset + 0x38);
	emu->sr[X86_R_GS].selector = x86_memory_read16(emu, offset + 0x3C);
	emu->sr[X86_R_FS].selector = x86_memory_read16(emu, offset + 0x40);
	emu->sr[X86_R_DS].selector = x86_memory_read16(emu, offset + 0x44);
	emu->sr[X86_R_SS].selector = x86_memory_read16(emu, offset + 0x48);
	emu->sr[X86_R_CS].selector = x86_memory_read16(emu, offset + 0x4C);
	emu->sr[X86_R_ES].selector = x86_memory_read16(emu, offset + 0x50);
	x86_descriptor_cache_read_386(emu, offset + 0x54, &emu->sr[X86_R_TR]);
	x86_descriptor_cache_read_386(emu, offset + 0x60, &emu->sr[X86_R_IDTR]);
	x86_descriptor_cache_read_386(emu, offset + 0x6C, &emu->sr[X86_R_GDTR]);
	x86_descriptor_cache_read_386(emu, offset + 0x78, &emu->sr[X86_R_LDTR]);
	x86_descriptor_cache_read_386(emu, offset + 0x84, &emu->sr[X86_R_GS]);
	x86_descriptor_cache_read_386(emu, offset + 0x90, &emu->sr[X86_R_FS]);
	x86_descriptor_cache_read_386(emu, offset + 0x9C, &emu->sr[X86_R_DS]);
	x86_descriptor_cache_read_386(emu, offset + 0xA8, &emu->sr[X86_R_SS]);
	x86_descriptor_cache_read_386(emu, offset + 0xB4, &emu->sr[X86_R_CS]);
	x86_descriptor_cache_read_386(emu, offset + 0xC0, &emu->sr[X86_R_ES]);
	x86_set_cpl(emu,  (emu->sr[X86_R_SS].access >> X86_DESC_DPL_SHIFT) & 3);
	emu->cpu_level = X86_LEVEL_USER;
}

//// SMM entry

static inline bool x86_io_instruction_has_rep_prefix(x86_io_type_t io_type)
{
	switch(io_type)
	{
	case X86_REP_OUTS:
	case X86_REP_INS:
		return true;
	default:
		return false;
	}
}

static inline bool x86_io_instruction_is_string(x86_io_type_t io_type)
{
	switch(io_type)
	{
	case X86_OUTS:
	case X86_INS:
	case X86_REP_OUTS:
	case X86_REP_INS:
		return true;
	default:
		return false;
	}
}

static inline void x86_smm_store_state32(x86_state_t * emu, uaddr_t offset, x86_smi_attributes_t attributes)
{
	bool is_ins_io = attributes.source == X86_SMISRC_IO;
	bool is_valid_io = false; // TODO
	bool ins_had_rep = attributes.source == X86_SMISRC_IO && x86_io_instruction_has_rep_prefix(attributes.io_type);
	bool is_io_string = attributes.source == X86_SMISRC_IO && x86_io_instruction_is_string(attributes.io_type);
	int io_type = attributes.io_type;
	uint16_t io_port = attributes.write_address;
	uint32_t io_address = 0; // TODO
	bool a20_line = true; // TODO
	int ins_size;
	switch(attributes.write_size)
	{
	case SIZE_8BIT:
		ins_size = 1;
		break;
	case SIZE_16BIT:
		ins_size = 2;
		break;
	case SIZE_32BIT:
		ins_size = 4;
		break;
	default:
		ins_size = 0;
		break;
	}

	switch(emu->cpu_traits.smm_format)
	{
	default:
		assert(false);

	case X86_SMM_80386SL:
		// Robert R. Collins: The Secrets of System Management Mode
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F85, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F48, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_386(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_ES]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F28, emu->cr[4]);
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F26, /* RSM control */); // TODO
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F24, emu->dr[6]); // TODO: alternate DR6
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi); // TODO: or CR0
		}
		break;
	case X86_SMM_P5:
		// Robert R. Collins: The Secrets of System Management Mode, sandpile.org
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F85, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F48, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_p5(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_ES]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F28, emu->cr[4]);
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F26, /* RSM control */); // TODO
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F24, emu->dr[6]); // TODO: alternate DR6
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi); // TODO: or CR0
		}
		break;
	case X86_SMM_P6:
		// sandpile.org
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F84, &emu->sr[X86_R_ES]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GDTR]);
		// TODO: undocumented field inbetween
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F5C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F50, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F44, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F38, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F2C, &emu->sr[X86_R_DS]);
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F26, /* RSM control */); // TODO
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F24, emu->dr[6]); // TODO: alternate DR6
		// TODO: a couple of undocumented fields
		x86_memory_write32(emu, offset - 0x8000 + 0x7F14, emu->cr[4]);
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi); // TODO: or CR0
		}
		break;
	case X86_SMM_P4:
		// sandpile.org
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_TR]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F68, x86_flags_get32(emu));
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F5C, &emu->sr[X86_R_LDTR]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F58, emu->sr[X86_R_IDTR].limit);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F54, emu->sr[X86_R_IDTR].base);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F50, emu->sr[X86_R_GDTR].limit);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F4C, emu->sr[X86_R_GDTR].base);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F40, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F34, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F28, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F1C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F10, &emu->sr[X86_R_CS]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F8C, a20_line ? 0x0000 : 0x3000);

		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA4,
				0x0001
				+ (ins_size << 1)
				+ (io_type << 4)
				+ ((uint32_t)io_port << 16));
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA0, io_address);

			x86_memory_write32(emu, offset - 0x8000 + 0x7F84, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F80, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F7C, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F78, emu->io_restart_xdi);
		}
		else
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA0, 0);
		}
		break;
	case X86_SMM_K5:
	case X86_SMM_K6: // TODO
		x86_memory_write32(emu, offset - 0x8000 + 0x7FA4,
			(is_ins_io ? 0x00000001 : 0)
			| (is_valid_io ? 0x00000002 : 0)
			| (is_io_string ? 0x00000004 : 0)
			| (ins_had_rep ? 0x00000008 : 0)
			| ((uint32_t)io_port << 16));
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F9E, emu->old_xip);
		}
		x86_descriptor_cache_write_k5_no_access(emu, offset - 0x8000 + 0x7F8C, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_k5_no_access(emu, offset - 0x8000 + 0x7F84, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F24, &emu->sr[X86_R_ES]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F14, emu->cr[2]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->cr[4]);
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi);
		}
		break;
	}

	x86_memory_write32(emu, offset - 0x8000 + 0x7FFC, emu->cr[0]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF8, emu->cr[3]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF4, x86_flags_get32(emu));
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF0, emu->xip);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FEC, emu->gpr[X86_R_DI]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE8, emu->gpr[X86_R_SI]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE4, emu->gpr[X86_R_BP]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE0, emu->gpr[X86_R_SP]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FDC, emu->gpr[X86_R_BX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD8, emu->gpr[X86_R_DX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD4, emu->gpr[X86_R_CX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD0, emu->gpr[X86_R_AX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FCC, emu->dr[6]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC8, emu->dr[7]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC4, emu->sr[X86_R_TR].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC0, emu->sr[X86_R_LDTR].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FBC, emu->sr[X86_R_GS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB8, emu->sr[X86_R_FS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB4, emu->sr[X86_R_DS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB0, emu->sr[X86_R_SS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FAC, emu->sr[X86_R_CS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FA8, emu->sr[X86_R_ES].selector);

	x86_memory_write16(emu, offset - 0x8000 + 0x7F02, emu->halted ? 1 : 0);
	x86_memory_write16(emu, offset - 0x8000 + 0x7F00, 0); // I/O trap slot
	x86_memory_write32(emu, offset - 0x8000 + 0x7EFC, emu->smm_revision_identifier);
	if((emu->smm_revision_identifier & SMM_REVID_SMBASE_RELOC) != 0)
		x86_memory_write32(emu, offset - 0x8000 + 0x7EF8, emu->smbase);
}

static inline void x86_smm_store_state64(x86_state_t * emu, uaddr_t offset, x86_smi_attributes_t attributes)
{
	bool is_ins_io = attributes.source == X86_SMISRC_IO;
	int io_type = attributes.io_type;
	uint16_t io_port = attributes.write_address;
	int ins_size;
	switch(attributes.write_size)
	{
	case SIZE_8BIT:
		ins_size = 1;
		break;
	case SIZE_16BIT:
		ins_size = 2;
		break;
	case SIZE_32BIT:
		ins_size = 4;
		break;
	default:
		ins_size = 0;
		break;
	}

	x86_memory_write64(emu, offset - 0x8000 + 0x7FF8, emu->gpr[X86_R_AX]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FF0, emu->gpr[X86_R_CX]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FE8, emu->gpr[X86_R_DX]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FE0, emu->gpr[X86_R_BX]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FD8, emu->gpr[X86_R_SP]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FD0, emu->gpr[X86_R_BP]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FC8, emu->gpr[X86_R_SI]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FC0, emu->gpr[X86_R_DI]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FB8, emu->gpr[8]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FB0, emu->gpr[9]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FA8, emu->gpr[10]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7FA0, emu->gpr[11]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F98, emu->gpr[12]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F90, emu->gpr[13]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F88, emu->gpr[14]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F80, emu->gpr[15]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F78, emu->xip);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F70, x86_flags_get64(emu));
	x86_memory_write64(emu, offset - 0x8000 + 0x7F68, emu->dr[6]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F60, emu->dr[7]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F58, emu->cr[0]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F50, emu->cr[3]);
	x86_memory_write64(emu, offset - 0x8000 + 0x7F48, emu->cr[4]);

	x86_memory_write32(emu, offset - 0x8000 + 0x7F00, emu->smbase);
	x86_memory_write32(emu, offset - 0x8000 + 0x7EFC, emu->smm_revision_identifier);

	x86_memory_write64(emu, offset - 0x8000 + 0x7ED0, emu->efer);

	x86_memory_write8(emu, offset - 0x8000 + 0x7ECB, emu->cpl);
	// TODO: block NMI
	x86_memory_write8(emu, offset - 0x8000 + 0x7EC9, emu->halted ? 1 : 0);
	x86_memory_write8(emu, offset - 0x8000 + 0x7EC8, 0); // I/O trap slot

	// TODO: guessing
	if(is_ins_io)
	{
		x86_memory_write32(emu, offset - 0x8000 + 0x7FC0,
			0x0001
			+ (ins_size << 1)
			+ (io_type << 4)
			+ ((uint32_t)io_port << 16));

		x86_memory_write32(emu, offset - 0x8000 + 0x7FB8, emu->io_restart_xdi);
		x86_memory_write32(emu, offset - 0x8000 + 0x7FB0, emu->io_restart_xsi);
		x86_memory_write32(emu, offset - 0x8000 + 0x7FA8, emu->io_restart_xcx);
		x86_memory_write32(emu, offset - 0x8000 + 0x7FA0, emu->old_xip);
	}
	else
	{
		x86_memory_write32(emu, offset - 0x8000 + 0x7FC0, 0);
	}

	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE90, &emu->sr[X86_R_TR]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE80, &emu->sr[X86_R_IDTR]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE70, &emu->sr[X86_R_LDTR]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE60, &emu->sr[X86_R_GDTR]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE50, &emu->sr[X86_R_GS]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE40, &emu->sr[X86_R_FS]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE30, &emu->sr[X86_R_DS]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE20, &emu->sr[X86_R_SS]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE10, &emu->sr[X86_R_CS]);
	x86_descriptor_cache_write_64(emu, offset - 0x8000 + 0xFE00, &emu->sr[X86_R_ES]);
}

static inline void x86_smm_store_state_cyrix(x86_state_t * emu, uaddr_t offset, x86_smi_attributes_t attributes)
{
	bool ins_had_rep = attributes.source == X86_SMISRC_IO && x86_io_instruction_has_rep_prefix(attributes.io_type);
	bool ins_is_write = attributes.source == X86_SMISRC_MEMORY;
	bool ins_smint = attributes.source == X86_SMISRC_SMINT;
	bool cpu_in_halt = emu->halted;
	bool memory_access = attributes.source == X86_SMISRC_MEMORY; // TODO: how is this different from ins_is_write?
	uint32_t write_address = attributes.write_address;
	uint32_t write_data = attributes.write_data;
	bool code_writable = x86_is_code_writable(emu);
	bool code_readable = x86_access_is_readable(emu->sr[X86_R_CS].access);
	bool internal_smi = attributes.source != X86_SMISRC_EXTERNAL;
	bool nested_smi = attributes.nested_smi;
	bool vga_access = attributes.vga_access;
	int write_size;
	switch(attributes.write_size)
	{
	case SIZE_8BIT:
		write_size = 1;
		break;
	case SIZE_16BIT:
		write_size = 3;
		break;
	case SIZE_32BIT:
		write_size = 15;
		break;
	default:
		break;
	}

	uint8_t code_descriptor[8];

	switch(emu->cpu_traits.smm_format)
	{
	default:
		assert(false);

	case X86_SMM_CX486SLCE:
		x86_segment_store_protected_mode_386(emu, X86_R_CS, code_descriptor);
		x86_memory_write(emu, offset - 0x20, 8, code_descriptor);

		x86_memory_write16(emu, offset - 0x24,
			(ins_is_write ? 0x0002 : 0)
			+ (ins_had_rep ? 0x0004 : 0));

		if(!memory_access && ins_had_rep)
			x86_memory_write32(emu, offset - 0x30, ins_is_write ? emu->io_restart_xsi : emu->io_restart_xdi);
		break;

	case X86_SMM_5X86:
		// TODO: layout unknown for 5x86, using that of 6x86

		x86_segment_store_protected_mode_386(emu, X86_R_CS, code_descriptor);
		x86_memory_write(emu, offset - 0x20, 8, code_descriptor);

		x86_memory_write16(emu, offset - 0x16, emu->cpl << 5);

		x86_memory_write16(emu, offset - 0x24,
			(ins_is_write ? 0x0002 : 0)
			+ (ins_had_rep ? 0x0004 : 0)
			+ (ins_smint ? 0x0008 : 0)
			+ (cpu_in_halt ? 0x0010 : 0));

		x86_memory_write16(emu, offset - 0x26, write_size);
		x86_memory_write16(emu, offset - 0x28, write_address);
		x86_memory_write16(emu, offset - 0x2C, write_data);

		if(!memory_access && ins_had_rep)
			x86_memory_write32(emu, offset - 0x30, ins_is_write ? emu->io_restart_xsi : emu->io_restart_xdi);

		break;

	case X86_SMM_M2:
		x86_segment_store_protected_mode_386(emu, X86_R_CS, code_descriptor);
		x86_memory_write(emu, offset - 0x20, 8, code_descriptor);

		x86_memory_write16(emu, offset - 0x22, emu->cpl << 5);

		x86_memory_write16(emu, offset - 0x24,
			(code_writable ? 0x0001 : 0)
			+ (ins_is_write ? 0x0002 : 0)
			+ (ins_had_rep ? 0x0004 : 0)
			+ (ins_smint ? 0x0008 : 0)
			+ (cpu_in_halt ? 0x0010 : 0)
			+ (internal_smi ? 0x2000 : 0)
			+ (nested_smi ? 0x8000 : 0));

		x86_memory_write16(emu, offset - 0x26, write_size);
		x86_memory_write16(emu, offset - 0x28, write_address);
		x86_memory_write16(emu, offset - 0x2C, write_data);

		if(!memory_access && ins_had_rep)
			x86_memory_write32(emu, offset - 0x30, ins_is_write ? emu->io_restart_xsi : emu->io_restart_xdi);

		break;

	case X86_SMM_MEDIAGX:
		x86_segment_store_protected_mode_386(emu, X86_R_CS, code_descriptor);
		x86_memory_write(emu, offset - 0x20, 8, code_descriptor);

		x86_memory_write16(emu, offset - 0x24,
			(code_writable ? 0x0001 : 0)
			+ (ins_is_write ? 0x0002 : 0)
			+ (ins_had_rep ? 0x0004 : 0)
			+ (ins_smint ? 0x0008 : 0)
			+ (cpu_in_halt ? 0x0010 : 0)
			+ (memory_access ? 0x0020 : 0)
			+ (!internal_smi ? 0x0040 : 0)
			+ (vga_access ? 0x0080 : 0)
			+ (nested_smi ? 0x0100 : 0));

		if(!memory_access)
		{
			x86_memory_write16(emu, offset - 0x26, write_size);
			x86_memory_write16(emu, offset - 0x28, write_address);
		}
		x86_memory_write16(emu, offset - 0x2C, write_data);
		if(!memory_access && ins_had_rep)
			x86_memory_write32(emu, offset - 0x30, ins_is_write ? emu->io_restart_xsi : emu->io_restart_xdi);
		if(memory_access)
		{
			x86_memory_write32(emu, offset - 0x34, write_address);
		}

		break;

	case X86_SMM_GX2:
		x86_memory_write16(emu, offset - 0x16, emu->sr[X86_R_CS].access >> 8);
		x86_memory_write32(emu, offset - 0x1C, emu->sr[X86_R_CS].base);
		x86_memory_write32(emu, offset - 0x1D, emu->sr[X86_R_CS].limit);
		x86_memory_write16(emu, offset - 0x22, emu->sr[X86_R_SS].access >> 8); // note: this is where LX reads the CPL back afterwards

		x86_memory_write16(emu, offset - 0x24,
			(code_writable ? 0x0001 : 0)
			+ (ins_is_write ? 0x0002 : 0)
			+ (ins_had_rep ? 0x0004 : 0)
			+ (ins_smint ? 0x0008 : 0)
			+ (cpu_in_halt ? 0x0010 : 0)
			+ (memory_access ? 0x0020 : 0)
			+ (!internal_smi ? 0x0040 : 0)
			+ (vga_access ? 0x0080 : 0)
			+ (nested_smi ? 0x0100 : 0)
			+ (code_readable ? 0x8000 : 0));

		if(!memory_access)
		{
			x86_memory_write16(emu, offset - 0x26, write_size);
			x86_memory_write16(emu, offset - 0x28, write_address);
		}
		x86_memory_write16(emu, offset - 0x2C, write_data);
		x86_memory_write32(emu, offset - 0x30, emu->smm_ctl);

		break;
	}

	x86_memory_write32(emu, offset - 0x04, emu->dr[7]);
	x86_memory_write32(emu, offset - 0x08, x86_flags_get32(emu));
	x86_memory_write32(emu, offset - 0x0C, emu->cr[0]);
	x86_memory_write32(emu, offset - 0x10, emu->old_xip);
	x86_memory_write32(emu, offset - 0x14, emu->xip);
	x86_memory_write16(emu, offset - 0x18, emu->sr[X86_R_CS].selector);
}

static inline void x86_smm_enter(x86_state_t * emu, x86_smi_attributes_t attributes)
{
	switch(emu->cpu_traits.smm_format)
	{
	case X86_SMM_NONE:
		assert(false);

	// Intel and AMD CPUs (32-bit)
	case X86_SMM_80386SL:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state32(emu, 0x30000 + 0xFFFF + 1, attributes);

		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->dr[7] &= ~0xFFFF03FF;
		emu->sr[X86_R_CS].selector = 0x3000;
		emu->sr[X86_R_CS].base = 0x30000;
		emu->sr[X86_R_CS].access = X86_DESC_R | X86_DESC_X | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		emu->xip = 0x8000;
		break;
	case X86_SMM_P5:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state32(emu, emu->smbase + 0xFFFF + 1, attributes);

		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->cr[4] = 0;
		emu->dr[7] = 0x00000400;
		emu->sr[X86_R_CS].selector = 0x3000;
		emu->sr[X86_R_CS].base = emu->smbase;
		emu->sr[X86_R_CS].access = X86_DESC_R | X86_DESC_X | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		emu->xip = 0x8000;
		break;
	case X86_SMM_P6:
	case X86_SMM_P4:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state32(emu, emu->smbase + 0xFFFF + 1, attributes);

		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->cr[4] = 0;
		emu->dr[7] = 0x00000400;

		emu->sr[X86_R_CS].selector = emu->smbase >> 4;
		emu->sr[X86_R_CS].base = emu->smbase;
		emu->sr[X86_R_CS].access = X86_DESC_R | X86_DESC_X | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		emu->xip = 0x8000;
		break;
	case X86_SMM_K5:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state32(emu, emu->smbase + 0xFFFF + 1, attributes);

		// TODO: check
		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->cr[4] = 0;
		emu->dr[7] = 0x00000400;

		emu->sr[X86_R_CS].selector = 0x3000;
		emu->sr[X86_R_CS].base = emu->smbase;
		emu->xip = 0x8000;
		break;
	case X86_SMM_K6:
		// TODO: check
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state32(emu, emu->smbase + 0xFFFF + 1, attributes);

		// TODO: check
		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->cr[4] = 0;
		emu->dr[7] = 0x00000400;

		emu->sr[X86_R_CS].selector = emu->smbase >> 4;
		emu->sr[X86_R_CS].base = emu->smbase;
		emu->xip = 0x8000;
		break;

	// Intel and AMD CPUs (64-bit)
	case X86_SMM_AMD64:
		// TODO: check
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state64(emu, emu->smbase + 0xFFFF + 1, attributes);

		// TODO: check
		x86_flags_set32(emu, 0x0002); // clear all flags
		for(x86_segnum_t segment = X86_R_ES; segment <= X86_R_GS; segment++)
		{
			emu->sr[segment].limit = 0xFFFFFFFF;
			if(segment == X86_R_CS)
				continue;
			emu->sr[segment].selector = 0;
			emu->sr[segment].base = 0;
			emu->sr[segment].access = X86_DESC_W | X86_DESC_S | X86_DESC_P | X86_DESC_G;
		}
		emu->cr[0] &= ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_PG);
		emu->cr[4] = 0;
		emu->dr[7] = 0x00000400;

		emu->sr[X86_R_CS].selector = emu->smbase >> 4;
		emu->sr[X86_R_CS].base = emu->smbase;
		emu->xip = 0x8000;
		break;

	// Cyrix CPUs and derivatives (32-bit)
	case X86_SMM_CX486SLCE:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state_cyrix(emu,
			((emu->arr[3] & 0xFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000),
			attributes);

		// Note: documentation refers to arr[3] as ARR4, but the I/O addresses are in the same location as in later CPUs
		emu->sr[X86_R_CS].selector = emu->arr[3] & 0xFFF0;
		emu->sr[X86_R_CS].base = (emu->arr[3] & 0xFFF0) << 12;
		emu->xip = 0;
		break;
	case X86_SMM_5X86:
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state_cyrix(emu,
			((emu->arr[3] & 0xFFFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000),
			attributes);

		emu->sr[X86_R_CS].selector = emu->arr[3] & 0xFFF0;
		emu->sr[X86_R_CS].base = (emu->arr[3] & 0xFFFFF0) << 12;
		emu->xip = 0;
		break;
	case X86_SMM_M2:
		attributes.nested_smi = emu->cpu_level == X86_LEVEL_SMM;
		emu->cpu_level = X86_LEVEL_SMM;
		if((emu->smm_hdr & 1) == 0)
			emu->smm_hdr = ((emu->arr[3] & 0xFFFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000) + 1;
		x86_smm_store_state_cyrix(emu, emu->smm_hdr, attributes);

		emu->sr[X86_R_CS].selector = emu->arr[3] & 0xFFF0;
		emu->sr[X86_R_CS].base = (emu->arr[3] & 0xFFFFF0) << 12;
		emu->xip = 0;
		break;
	case X86_SMM_MEDIAGX:
		attributes.nested_smi = emu->cpu_level == X86_LEVEL_SMM;
		emu->cpu_level = X86_LEVEL_SMM;
		if((emu->smm_hdr & 1) == 0)
			emu->smm_hdr = ((emu->arr[3] & 0xFFFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000) + 1;
		x86_smm_store_state_cyrix(emu, emu->smm_hdr, attributes);

		emu->sr[X86_R_CS].selector = emu->arr[3] & 0xFFF0;
		emu->sr[X86_R_CS].base = (emu->arr[3] & 0xFFFFF0) << 12;
		emu->xip = 0;
		break;
	case X86_SMM_GX2:
		attributes.nested_smi = emu->cpu_level == X86_LEVEL_SMM;
		emu->cpu_level = X86_LEVEL_SMM;
		x86_smm_store_state_cyrix(emu, emu->smm_hdr, attributes);

		emu->sr[X86_R_CS].selector = emu->smm.base >> 4;
		emu->sr[X86_R_CS].base = emu->smm.base;
		if(emu->smm.limit < 0x100000)
		{
			emu->sr[X86_R_CS].limit = emu->smm.limit;
			emu->sr[X86_R_CS].access &= ~X86_DESC_G;
		}
		else
		{
			emu->sr[X86_R_CS].limit = emu->smm.limit | 0xFFF;
			emu->sr[X86_R_CS].access |= X86_DESC_G;
		}
		emu->xip = 0;
		break;
	}
}

// Resume from SMI

static inline void x86_smm_restore_state32(x86_state_t * emu, uaddr_t offset)
{
	(void) emu;
	(void) offset;
	// TODO
#if 0
	switch(emu->cpu_traits.smm_format)
	{
	default:
		assert(false);

	case X86_SMM_80386SL:
		// Robert R. Collins: The Secrets of System Management Mode
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F85, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F48, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_read_386(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_ES]);
		emu->cr[4] = x86_memory_read32(emu, offset - 0x8000 + 0x7F28);
		if((emu->smm_revision_identifier & SMM_REVID_IO_RESTART) != 0 && x86_memory_read8(emu, offset - 0x8000 + 0x7F00, 0) == 0xFF)
		{
			emu->xip = x86_memory_read32(emu, offset - 0x8000 + 0x7F10);
			emu->gpr[X86_R_SI] = x86_memory_read32(emu, offset - 0x8000 + 0x7F1C);
			emu->gpr[X86_R_CX] = x86_memory_read32(emu, offset - 0x8000 + 0x7F18);
			emu->gpr[X86_R_DI] = x86_memory_read32(emu, offset - 0x8000 + 0x7F14);
		}
		break;
	case X86_SMM_P5:
		// Robert R. Collins: The Secrets of System Management Mode, sandpile.org
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F85, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F48, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_read_p5(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_ES]);
		emu->cr[4] = x86_memory_read32(emu, offset - 0x8000 + 0x7F28);
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F26, /* RSM control */); // TODO
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F24, emu->dr[6]); // TODO: alternate DR6
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi); // TODO: or CR0
		}
		break;
	case X86_SMM_P6:
		// sandpile.org
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F9C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F90, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F84, &emu->sr[X86_R_ES]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_GDTR]);
		// TODO: undocumented field inbetween
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F5C, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F50, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F44, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F38, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_p6(emu, offset - 0x8000 + 0x7F2C, &emu->sr[X86_R_DS]);
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F26, /* RSM control */); // TODO
		//x86_memory_write16(emu, offset - 0x8000 + 0x7F24, emu->dr[6]); // TODO: alternate DR6
		// TODO: a couple of undocumented fields
		emu->cr[4] = x86_memory_read32(emu, offset - 0x8000 + 0x7F14);
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi); // TODO: or CR0
		}
		break;
	case X86_SMM_P4:
		// sandpile.org
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_TR]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F68, x86_flags_get32(emu));
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F5C, &emu->sr[X86_R_LDTR]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F58, emu->sr[X86_R_IDTR].limit);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F54, emu->sr[X86_R_IDTR].base);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F50, emu->sr[X86_R_GDTR].limit);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F4C, emu->sr[X86_R_GDTR].base);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F40, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F34, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F28, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F1C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_p4(emu, offset - 0x8000 + 0x7F10, &emu->sr[X86_R_CS]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F8C, a20_line ? 0x0000 : 0x3000);

		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA4,
				0x0001
				+ (ins_size << 1)
				+ (io_type << 4)
				+ ((uint32_t)io_port << 16));
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA0, io_address);

			x86_memory_write32(emu, offset - 0x8000 + 0x7F84, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F80, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F7C, emu->old_xip);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F78, emu->io_restart_xdi);
		}
		else
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7FA0, 0);
		}
		break;
	case X86_SMM_K5:
	case X86_SMM_K6: // TODO
		x86_memory_write32(emu, offset - 0x8000 + 0x7FA4,
			(is_ins_io ? 0x00000001 : 0)
			| (is_valid_io ? 0x00000002 : 0)
			| (is_io_string ? 0x00000004 : 0)
			| (ins_had_rep ? 0x00000008 : 0)
			| ((uint32_t)io_port << 16));
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F9E, emu->old_xip);
		}
		x86_descriptor_cache_write_k5_no_access(emu, offset - 0x8000 + 0x7F8C, &emu->sr[X86_R_IDTR]);
		x86_descriptor_cache_write_k5_no_access(emu, offset - 0x8000 + 0x7F84, &emu->sr[X86_R_GDTR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F78, &emu->sr[X86_R_TR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F6C, &emu->sr[X86_R_LDTR]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_GS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F54, &emu->sr[X86_R_FS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F60, &emu->sr[X86_R_DS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F3C, &emu->sr[X86_R_SS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F30, &emu->sr[X86_R_CS]);
		x86_descriptor_cache_write_k5(emu, offset - 0x8000 + 0x7F24, &emu->sr[X86_R_ES]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F14, emu->cr[2]);
		x86_memory_write32(emu, offset - 0x8000 + 0x7F10, emu->cr[4]);
		if(is_ins_io)
		{
			x86_memory_write32(emu, offset - 0x8000 + 0x7F0C, emu->io_restart_xsi);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F08, emu->io_restart_xcx);
			x86_memory_write32(emu, offset - 0x8000 + 0x7F04, emu->io_restart_xdi);
		}
		break;
	}

	x86_memory_write32(emu, offset - 0x8000 + 0x7FFC, emu->cr[0]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF8, emu->cr[3]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF4, x86_flags_get32(emu));
	x86_memory_write32(emu, offset - 0x8000 + 0x7FF0, emu->xip);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FEC, emu->gpr[X86_R_DI]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE8, emu->gpr[X86_R_SI]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE4, emu->gpr[X86_R_BP]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FE0, emu->gpr[X86_R_SP]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FDC, emu->gpr[X86_R_BX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD8, emu->gpr[X86_R_DX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD4, emu->gpr[X86_R_CX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FD0, emu->gpr[X86_R_AX]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FCC, emu->dr[6]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC8, emu->dr[7]);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC4, emu->sr[X86_R_TR].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FC0, emu->sr[X86_R_LDTR].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FBC, emu->sr[X86_R_GS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB8, emu->sr[X86_R_FS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB4, emu->sr[X86_R_DS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FB0, emu->sr[X86_R_SS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FAC, emu->sr[X86_R_CS].selector);
	x86_memory_write32(emu, offset - 0x8000 + 0x7FA8, emu->sr[X86_R_ES].selector);

	x86_memory_write16(emu, offset - 0x8000 + 0x7F02, emu->halted ? 1 : 0);
	x86_memory_write16(emu, offset - 0x8000 + 0x7F00, 0); // I/O trap slot
	x86_memory_write32(emu, offset - 0x8000 + 0x7EFC, emu->smm_revision_identifier);
	if((emu->smm_revision_identifier & SMM_REVID_SMBASE_RELOC) != 0)
		x86_memory_write32(emu, offset - 0x8000 + 0x7EF8, emu->smbase);
#endif
}

static inline void x86_smm_restore_state64(x86_state_t * emu, uaddr_t offset)
{
	(void) emu;
	(void) offset;
	// TODO
}

static inline void x86_smm_restore_state_cyrix(x86_state_t * emu, uaddr_t offset)
{
	(void) emu;
	(void) offset;
	// TODO
}

static inline void x86_smm_resume(x86_state_t * emu)
{
	switch(emu->cpu_traits.smm_format)
	{
	case X86_SMM_NONE:
		assert(false);

	// Intel and AMD CPUs (32-bit)
	case X86_SMM_80386SL:
		x86_smm_restore_state32(emu, 0x30000 + 0xFFFF + 1);
		break;
	case X86_SMM_P5:
	case X86_SMM_P6:
	case X86_SMM_P4:
	case X86_SMM_K5:
	case X86_SMM_K6:
		x86_smm_restore_state32(emu, emu->smbase + 0xFFFF + 1);
		break;

	// Intel and AMD CPUs (64-bit)
	case X86_SMM_AMD64:
		x86_smm_restore_state64(emu, emu->smbase + 0xFFFF + 1);
		break;

	// Cyrix CPUs and derivatives (32-bit)
	case X86_SMM_CX486SLCE:
		x86_smm_restore_state_cyrix(emu,
			((emu->arr[3] & 0xFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000));
		break;
	case X86_SMM_5X86:
		x86_smm_restore_state_cyrix(emu,
			((emu->arr[3] & 0xFFFFF0) << 12) + ((emu->arr[3] & 0xF) < 15 ? 0x2000 << (emu->arr[3] & 0xF) : 0x100000000));
		break;
	case X86_SMM_M2:
	case X86_SMM_MEDIAGX:
	case X86_SMM_GX2:
		x86_smm_restore_state_cyrix(emu, emu->smm_hdr);
		break;
	}
}

// Checks

static inline bool x86_smm_instruction_valid(x86_state_t * emu)
{
	switch(emu->cpu_traits.smm_format)
	{
	case X86_SMM_NONE:
		return false;

	case X86_SMM_80386SL:
	case X86_SMM_P5:
	case X86_SMM_P6:
	case X86_SMM_P4:
	case X86_SMM_K5:
	case X86_SMM_K6:
	case X86_SMM_AMD64:
		// TODO: anything else? double check
	default:
		return emu->cpu_level == X86_LEVEL_SMM;

	case X86_SMM_CX486SLCE:
		return emu->cpl == 0 && (emu->cpu_level == X86_LEVEL_SMM || (emu->ccr[1] & X86_CCR1_SMAC) != 0);

	case X86_SMM_5X86:
		// TODO: this is the 6x86
		return emu->cpl == 0 && (emu->arr[3] & 0xF) != 0 && (emu->cpu_level == X86_LEVEL_SMM || (emu->ccr[1] & X86_CCR1_SMAC) != 0) && (emu->ccr[1] & X86_CCR1_SM3) != 0;

	case X86_SMM_M2:
		// TODO: Cyrix III ignores SMAC bit
		return emu->cpl == 0 && (emu->arr[3] & 0xF) != 0 && (emu->cpu_level == X86_LEVEL_SMM || (emu->ccr[1] & X86_CCR1_SMAC) != 0) && (emu->ccr[1] & (X86_CCR1_USE_SMI|X86_CCR1_SM3)) != 0;

	case X86_SMM_MEDIAGX:
		return emu->cpl == 0 && (emu->arr[3] & 0xF) != 0 && (emu->cpu_level == X86_LEVEL_SMM || (emu->ccr[1] & X86_CCR1_SMAC) != 0) && (emu->ccr[1] & X86_CCR1_USE_SMI) != 0;

	case X86_SMM_GX2:
		return emu->cpl == 0 && ((emu->smm_ctl & X86_SMI_INST) != 0 || emu->cpu_level != X86_LEVEL_USER);
	}
}

static inline bool x86_dmm_instruction_valid(x86_state_t * emu)
{
	if(emu->cpu_traits.smm_format != X86_SMM_GX2)
	{
		return false;
	}
	else
	{
		return emu->cpl == 0 && ((emu->dmi_ctl & X86_DMI_INST) != 0 || emu->cpu_level != X86_LEVEL_USER);
	}
}


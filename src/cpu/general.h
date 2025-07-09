
// registers

static inline void x86_set_xip(x86_state_t * emu, uoff_t value);

static inline uint16_t x86_register_get16(x86_state_t * emu, int number);
static inline uint32_t x86_register_get32(x86_state_t * emu, int number);
static inline uint64_t x86_register_get64(x86_state_t * emu, int number);

static inline void x86_register_set16(x86_state_t * emu, int number, uint16_t value);
static inline void x86_register_set32(x86_state_t * emu, int number, uint32_t value);
static inline void x86_register_set64(x86_state_t * emu, int number, uint64_t value);

static inline uint16_t x86_flags_get16(x86_state_t * emu);
static inline uint32_t x86_flags_get32(x86_state_t * emu);

static inline uint16_t x86_flags_get_image16(x86_state_t * emu);
static inline uint32_t x86_flags_get_image32(x86_state_t * emu);

static inline void x86_flags_set16(x86_state_t * emu, uint16_t value);
static inline void x86_flags_set32(x86_state_t * emu, uint32_t value);
void x86_flags_set64(x86_state_t * emu, uint64_t value);

static inline x86_segnum_t x86_segment_get_number(x86_state_t * emu, x86_segnum_t segment_number);
static inline void x86_segment_load_real_mode(x86_state_t * emu, x86_segnum_t segment, uint16_t value);
static inline void x86_segment_load_real_mode_full(x86_state_t * emu, x86_segnum_t segment, uint16_t value);

static inline uint8_t x86_cyrix_register_get(x86_state_t * emu, uint8_t number);
static inline void x86_cyrix_register_set(x86_state_t * emu, uint8_t number, uint8_t value);

static inline void x86_store_register_bank(x86_state_t * emu);
static inline void x86_load_register_bank(x86_state_t * emu);

static inline void x86_load_x80_registers(x86_state_t * emu);
static inline void x86_store_x80_registers(x86_state_t * emu);

// protection

static inline unsigned x86_get_cpl(x86_state_t * emu);
static inline void x86_set_cpl(x86_state_t * emu, unsigned rpl);

static inline bool x86_access_is_readable(uint16_t access);

static inline void x86_segment_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uoff_t size, uoff_t error_code);
static inline void x87_segment_check_limit(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uoff_t size, uoff_t error_code);
static inline void x86_segment_check_read(x86_state_t * emu, x86_segnum_t segment_number);
static inline void x86_segment_check_write(x86_state_t * emu, x86_segnum_t segment_number);

static inline void x86_segment_store_protected_mode_386(x86_state_t * emu, x86_segnum_t segment_number, uint8_t * descriptor);

static inline noreturn void x86_trigger_interrupt(x86_state_t * emu, int exception, uoff_t error_code);

static inline uint8_t x80_memory_read8(x80_state_t * emu, uint16_t address);
static inline uint16_t x80_memory_read16(x80_state_t * emu, uint16_t address);
static inline void x80_memory_write8(x80_state_t * emu, uint16_t address, uint8_t value);
static inline void x80_memory_write16(x80_state_t * emu, uint16_t address, uint16_t value);
static inline uint8_t x80_memory_fetch8(x80_state_t * emu);
static inline uint16_t x80_memory_fetch16(x80_state_t * emu);
static inline uint8_t x80_input8(x80_state_t * emu, uint16_t port);
static inline void x80_output8(x80_state_t * emu, uint16_t port, uint8_t value);

// memory

static inline void x86_check_canonical_address(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, uoff_t error_code);

static inline void x86_memory_segmented_read(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uaddr_t count, void * buffer);
static inline void x86_memory_segmented_write(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uaddr_t count, const void * buffer);

static inline uint8_t x86_memory_segmented_read8(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);
static inline uint16_t x86_memory_segmented_read16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);
static inline uint32_t x86_memory_segmented_read32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);
static inline uint64_t x86_memory_segmented_read64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);

static inline uint16_t x87_memory_segmented_read16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset);
static inline uint32_t x87_memory_segmented_read32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset);
static inline float80_t x87_memory_segmented_read80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset);

static inline void x86_memory_segmented_write8(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint8_t value);
static inline void x86_memory_segmented_write16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint16_t value);
static inline void x86_memory_segmented_write32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint32_t value);
static inline void x86_memory_segmented_write64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint64_t value);

static inline void x87_memory_segmented_write16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uint16_t value);
static inline void x87_memory_segmented_write32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uint32_t value);
static inline void x87_memory_segmented_write80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, float80_t value);

static inline uint8_t x86_memory_segmented_read8_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);
static inline uint16_t x86_memory_segmented_read16_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);
static inline uint32_t x86_memory_segmented_read32_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset);

static inline void x86_input(x86_state_t * emu, uint16_t port, uint16_t count, void * buffer);
static inline void x86_output(x86_state_t * emu, uint16_t port, uint16_t count, const void * buffer);

static inline void x86_prefetch_queue_flush(x86_state_t * emu);
static inline void x86_prefetch_queue_fill(x86_state_t * emu);

// smm

static inline void x86_ice_storeall_386(x86_state_t * emu, uaddr_t offset);

// x87

static inline void x87_convert_from_float80(float80_t value, uint64_t * fraction, uint16_t * exponent, bool * sign);
static inline float80_t x87_convert_to_float80(uint64_t fraction, uint16_t exponent, bool sign);
static inline float80_t x87_convert64_to_float(uint64_t value);

// x86

static inline uint8_t x86_fetch8(x86_parser_t * prs, x86_state_t * emu);
static inline uint16_t x86_fetch16(x86_parser_t * prs, x86_state_t * emu);
static inline uint32_t x86_fetch32(x86_parser_t * prs, x86_state_t * emu);
static inline uint64_t x86_fetch64(x86_parser_t * prs, x86_state_t * emu);

static inline void x86_push16(x86_state_t * emu, uint16_t value);
static inline void x86_push32(x86_state_t * emu, uint32_t value);
static inline void x86_push64(x86_state_t * emu, uint64_t value);

static inline uint16_t x86_pop16(x86_state_t * emu);
static inline uint32_t x86_pop32(x86_state_t * emu);
static inline uint64_t x86_pop64(x86_state_t * emu);

static inline uoff_t x86_get_stack_pointer(x86_state_t * emu);
static inline void x86_stack_adjust(x86_state_t * emu, uoff_t value);

// common

static inline bool x86_overflow(uaddr_t base, uaddr_t count, uaddr_t limit)
{
	return base > UADDR_MAX - count || base + (count - 1) > limit;
}

static inline void debug_printf(char debug_output[256], const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	size_t length = strlen(debug_output);
	vsnprintf(&debug_output[length], 256 - length, fmt, ap);
	va_end(ap);
}

static inline bool x86_is_intel64(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_INTEL;
}

static inline bool x86_segment_is_big(x86_segment_t * seg)
{
	return (seg->access & X86_DESC_D) != 0;
}

static inline bool x86_is_code_writable(x86_state_t * emu)
{
	// TODO: my current understanding is that the code segment is writable only if this is real mode/virtual 8086 mode, but sandpile.org and AMD Geode LX documentation indicates this might happen otherwise as well
	return x86_is_real_mode(emu) || x86_is_virtual_8086_mode(emu);
}

static inline x86_operation_size_t x86_get_stack_size(x86_state_t * emu)
{
	if(x86_is_64bit_mode(emu))
	{
		return SIZE_64BIT;
	}
	else if(x86_segment_is_big(&emu->sr[X86_R_SS]))
	{
		return SIZE_32BIT;
	}
	else
	{
		return SIZE_16BIT;
	}
}

static inline bool x86_stay_in_protected_mode(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_286 || (emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376);
}

static inline noreturn void x86_v60_exception(x86_state_t * emu, int exception)
{
	// TODO
	emu->emulation_result = X86_RESULT(X86_RESULT_CPU_INTERRUPT, exception);
	longjmp(emu->exc, 1);
	for(;;);
}

static inline noreturn void x86_ia64_intercept(x86_state_t * emu, int exception)
{
	// TODO
	x86_set_xip(emu, emu->old_xip);
	emu->emulation_result = X86_RESULT(X86_RESULT_CPU_INTERRUPT, exception);
	longjmp(emu->exc, 1);
	for(;;);
}

static const char * _x86_register_name8[]; // 8086 names come first, then the versions when a REX prefix is present
#define x86_register_name8(prs, number) (_x86_register_name8[(number) + ((prs)->rex_prefix ? 8 : 0)])
static const char * _x86_register_name16[]; // Intel names come first, followed by the NEC names
#define x86_register_name16(prs, number) (_x86_register_name16[(number) + ((prs)->use_nec_syntax ? 32 : 0)])
static const char * _x86_register_name32[];
#define x86_register_name32(prs, number) (_x86_register_name32[(number)])
static const char * _x86_register_name64[];
#define x86_register_name64(prs, number) (_x86_register_name64[(number)])
static const char * _x86_segment_name[]; // NEC names come first, followed by Intel names
#define x86_segment_name(prs, number) (_x86_segment_name[(number) + ((prs)->use_nec_syntax ? 0 : 8)])


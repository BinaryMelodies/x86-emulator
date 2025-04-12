
#include <stdbool.h>
#include <string.h>
#include "../src/cpu/cpu.h"

enum
{
	_AX,
	_CX,
	_DX,
	_BX,
	_SP,
	_BP,
	_SI,
	_DI,

	_ES,
	_CS,
	_SS,
	_DS,

	_IP,
	_FLAGS,
	_REGCOUNT,

	_AX_BIT = 1 << _AX,
	_CX_BIT = 1 << _CX,
	_DX_BIT = 1 << _DX,
	_BX_BIT = 1 << _BX,
	_SP_BIT = 1 << _SP,
	_BP_BIT = 1 << _BP,
	_SI_BIT = 1 << _SI,
	_DI_BIT = 1 << _DI,

	_ES_BIT = 1 << _ES,
	_CS_BIT = 1 << _CS,
	_SS_BIT = 1 << _SS,
	_DS_BIT = 1 << _DS,

	_IP_BIT = 1 << _IP,
	_FLAGS_BIT = 1 << _FLAGS,
};

struct mismatch
{
	uint16_t expected;
	uint16_t actual;
} regs[_REGCOUNT];

const char regnames[] = "ax\0cx\0dx\0bx\0sp\0bp\0si\0di\0es\0cs\0ss\0ds\0ip\0flags";

static uint8_t memory[0x100000];
static unsigned memory_changes;
static uint8_t memory_changed[0x100000];
static uint8_t memory_expected[0x100000];

void memory_read(x86_state_t * emu, x86_cpu_level_t level, uaddr_t address, void * buffer, size_t count)
{
	memcpy(buffer, &memory[address], count);
}

void memory_write(x86_state_t * emu, x86_cpu_level_t level, uaddr_t address, const void * buffer, size_t count)
{
	for(uint32_t i = address; i < address + count; i++)
	{
		if(memory_changed[i])
		{
			memory_changes --;
		}
		memory_changed[i] = (((uint8_t *)buffer)[i - address] != memory_expected[i]);
		if(memory_changed[i])
		{
			memory_changes ++;
		}
	}
	memcpy(&memory[address], buffer, count);
}

void port_read(x86_state_t * emu, uint16_t port, void * buffer, size_t count)
{
	memset(buffer, 0xFF, count);
}

void port_write(x86_state_t * emu, uint16_t port, const void * buffer, size_t count)
{
}

void memory_write_initial(x86_state_t * emu, uint32_t address, uint8_t value)
{
	memory[address] = value;
	memory_expected[address] = value;
	memory_changed[address] = false;
}

void memory_write_final(x86_state_t * emu, uint32_t address, uint8_t value)
{
	memory_expected[address] = value;
	memory_changed[address] = memory_expected[address] != memory[address];
	if(memory_changed[address])
		memory_changes ++;
}

struct mem
{
	uint32_t address : 20;
	uint8_t value;
	bool terminate;
};

struct regs
{
	uint16_t ax, cx, dx, bx, sp, bp, si, di;
	uint16_t es, cs, ss, ds, ip, flags;
};

struct testcase
{
	struct regs iregs, oregs;
	uint16_t flags_mask;
	struct mem * imem;
	struct mem * omem;
};

#include GENFILE

void run_tests(x86_state_t * emu)
{
	unsigned mismatch = 0;
#define START_TEST() do { mismatch = 0; if(memory_changes != 0) memset(memory_changed, 0, sizeof memory_changed); memory_changes = 0; } while(0)
#define FINISH_TEST() if(mismatch != 0 || memory_changes != 0) { printf("Test %d mismatches found\n", i); for(int i = 0; i < _REGCOUNT; i++) if((mismatch & (1 << i)) != 0) { printf("%s exp: 0x%04X act: 0x%04X\n", &regnames[3 * i], regs[i].expected, regs[i].actual); } for(uint32_t i = 0; i < 0x100000; i++) { if(memory_changed[i]) { printf("%05X exp: 0x%02X act: 0x%02X\n", i, memory_expected[i], memory[i]); memory_changed[i] = false; if(--memory_changes == 0) break; } } }
#define ASSERT(__reg, __var, __val) if((__var) != (__val)) { mismatch |= _##__reg##_BIT; regs[_##__reg] = (struct mismatch) { (__val), (__var) }; }
#define ASSERT_FLAGS(__emu, __val, __mask) ASSERT(FLAGS, x86_flags_get64(__emu) & (__mask), (__val) & (__mask))

	for(size_t i = 0; i < sizeof testcases / sizeof testcases[0]; i++)
	{
		emu->parser->debug_output[0] = '\0';
		//printf("Test %d\n", i);
		emu->ax = testcases[i].iregs.ax;
		emu->cx = testcases[i].iregs.cx;
		emu->dx = testcases[i].iregs.dx;
		emu->bx = testcases[i].iregs.bx;
		emu->sp = testcases[i].iregs.sp;
		emu->bp = testcases[i].iregs.bp;
		emu->si = testcases[i].iregs.si;
		emu->di = testcases[i].iregs.di;
		emu->es = testcases[i].iregs.es;
		emu->es_cache.base = (uint32_t)testcases[i].iregs.es << 4;
		emu->cs = testcases[i].iregs.cs;
		emu->cs_cache.base = (uint32_t)testcases[i].iregs.cs << 4;
		emu->ss = testcases[i].iregs.ss;
		emu->ss_cache.base = (uint32_t)testcases[i].iregs.ss << 4;
		emu->ds = testcases[i].iregs.ds;
		emu->ds_cache.base = (uint32_t)testcases[i].iregs.ds << 4;
		emu->ip = testcases[i].iregs.ip;
		x86_flags_set64(emu, testcases[i].iregs.flags);
		for(size_t addrix = 0; !testcases[i].imem[addrix].terminate; addrix++)
		{
			memory_write_initial(emu, testcases[i].imem[addrix].address, testcases[i].imem[addrix].value);
		}
		START_TEST();
		for(size_t addrix = 0; !testcases[i].omem[addrix].terminate; addrix++)
		{
			memory_write_final(emu, testcases[i].omem[addrix].address, testcases[i].omem[addrix].value);
		}
		//emu->parser->debug_output[0] = '\0';
		//emu->option_disassemble = true;
		while(true)
		{
			x86_result_t result = x86_step(emu);
			if(X86_RESULT_TYPE(result) != X86_RESULT_STRING)
				break;
		}
		//printf("%s", emu->parser->debug_output);
		ASSERT(AX, emu->ax, testcases[i].oregs.ax);
		ASSERT(CX, emu->cx, testcases[i].oregs.cx);
		ASSERT(DX, emu->dx, testcases[i].oregs.dx);
		ASSERT(BX, emu->bx, testcases[i].oregs.bx);
		ASSERT(SP, emu->sp, testcases[i].oregs.sp);
		ASSERT(BP, emu->bp, testcases[i].oregs.bp);
		ASSERT(SI, emu->si, testcases[i].oregs.si);
		ASSERT(DI, emu->di, testcases[i].oregs.di);
		ASSERT(ES, emu->es, testcases[i].oregs.es);
		ASSERT(CS, emu->cs, testcases[i].oregs.cs);
		ASSERT(SS, emu->ss, testcases[i].oregs.ss);
		ASSERT(DS, emu->ds, testcases[i].oregs.ds);
		ASSERT(IP, emu->ip, testcases[i].oregs.ip);
		ASSERT_FLAGS(emu, testcases[i].oregs.flags, testcases[i].flags_mask);
		FINISH_TEST();
	}
}

int main()
{
	x86_state_t emu[1];
	memset(&emu, 0, sizeof(emu));
	emu->cpu_type = X86_CPU_8086;
	emu->memory_read = memory_read;
	emu->memory_write = memory_write;
	emu->port_read = port_read;
	emu->port_write = port_write;
	x86_reset(emu, true);
	run_tests(emu);
	return 0;
}


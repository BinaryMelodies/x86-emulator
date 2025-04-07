
// Main file for x86 emulation (also includes x87, 8080/Z80 and 8089 emulation)

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include "cpu.h"

#include "general.h"
#include "registers.c"
#include "protection.c"
#include "memory.c"
#include "smm.c"
#include "x87.c"
#include "x86.c"
#include "x80.c"
#include "x89.c"

//// Register names

static const char * _x86_register_name8[] =
{
	// 8086 names
	"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
	// REX prefix names
	"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
	"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
	"r16b", "r17b", "r18b", "r19b", "r20b", "r21b", "r22b", "r23b",
	"r24b", "r25b", "r26b", "r27b", "r28b", "r29b", "r30b", "r31b"
};
static const char * _x86_register_name16[] =
{
	// Intel names
	"ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
	"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
	"r16w", "r17w", "r18w", "r19w", "r20w", "r21w", "r22w", "r23w",
	"r24w", "r25w", "r26w", "r27w", "r28w", "r29w", "r30w", "r31w",
	// NEC names
	"aw", "cw", "dw", "bw", "sp", "bp", "ix", "iy",
};
static const char * _x86_register_name32[] =
{
	"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
	"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
	"r16d", "r17d", "r18d", "r19d", "r20d", "r21d", "r22d", "r23d",
	"r24d", "r25d", "r26d", "r27d", "r28d", "r29d", "r30d", "r31d"
};
static const char * _x86_register_name64[] =
{
	"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};
static const char * _x86_segment_name[] =
{
	// NEC names
	"ds1", "ps", "ss", "ds0", "fs", "gs", "ds3", "ds2",
	// Intel names
	"es",  "cs", "ss", "ds",  "fs", "gs", "ds3", "ds2",
	// NEC name
	"iram"
};

static uint8_t _x80_fetch8(x80_parser_t * prs)
{
	x80_state_t * emu = (x80_state_t *)prs; // TODO: can we do this?

	return x80_memory_fetch8(emu);
}

static uint16_t _x80_fetch16(x80_parser_t * prs)
{
	x80_state_t * emu = (x80_state_t *)prs; // TODO: can we do this?

	return x80_memory_fetch16(emu);
}

static uint8_t _x86_fetch8(x86_parser_t * prs)
{
	x86_state_t * emu = (x86_state_t *)prs;

	return x86_fetch8(prs, emu);
}

static uint16_t _x86_fetch16(x86_parser_t * prs)
{
	x86_state_t * emu = (x86_state_t *)prs;

	return x86_fetch16(prs, emu);
}

static uint32_t _x86_fetch32(x86_parser_t * prs)
{
	x86_state_t * emu = (x86_state_t *)prs;

	return x86_fetch32(prs, emu);
}

static uint64_t _x86_fetch64(x86_parser_t * prs)
{
	x86_state_t * emu = (x86_state_t *)prs;

	return x86_fetch64(prs, emu);
}

static const uint8_t x86_flags_p[0x256] =
{
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
	0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
};

void x87_reset(x86_state_t * emu, bool reset)
{
	if(reset)
	{
		emu->x87.dw = 0; // optionally, X87_DW_S
		emu->x87.sg = 0x2310; // Intel387 SL Mobile Math CoProcessor A-0 (https://web.archive.org/web/20220107003754/https://datasheet.datasheetarchive.com/originals/scans/Scans-101/DSAIHSC000149849.pdf)
	}

	if(emu->cpu_type < X86_CPU_586)
	{
		emu->x87.cw = 0x037F;
	}
	else
	{
		emu->x87.cw = 0x0040;
	}
	if(emu->x87.fpu_type == X87_FPU_387 && reset)
	{
		emu->x87.sw = 0x0081; // enable ES and IE (invalid exception) to signal 80387 instead of 80287
	}
	else
	{
		emu->x87.sw = 0;
	}
	if(emu->cpu_type < X86_CPU_586)
	{
		emu->x87.tw = 0xFFFF; // all empty
	}
	else
	{
		emu->x87.tw = 0x5555; // all zeroes
	}
	emu->x87.fip = 0;
	emu->x87.fcs = 0;
	emu->x87.fdp = 0;
	emu->x87.fds = 0;
	emu->x87.fop = 0;

	emu->x87.next_fop = 0;
}

void x80_reset(x80_state_t * emu, bool reset)
{
	if(reset)
	{
		emu->parser->fetch8 = _x80_fetch8;
		emu->parser->fetch16 = _x80_fetch16;
	}

	emu->pc = 0;
	emu->iff1 = emu->iff2 = 0;

	// Z80 specific
	emu->i = emu->r = 0;
	emu->im = 0;
	emu->af_bank = emu->main_bank = 0;

	// 8085 specific
	emu->m5_5 = emu->m6_5 = emu->m7_5 = 1;
}

static uint8_t default_opcode_translation_table[256];

void x86_reset(x86_state_t * emu, bool reset)
{
	if(reset)
	{
		emu->parser->fetch8 = _x86_fetch8;
		emu->parser->fetch16 = _x86_fetch16;
		emu->parser->fetch32 = _x86_fetch32;
		emu->parser->fetch64 = _x86_fetch64;

		emu->x80.parser->fetch8 = _x80_fetch8;
		emu->x80.parser->fetch16 = _x80_fetch16;

		if(emu->x87.fpu_type == X87_FPU_INTEGRATED)
		{
			emu->cpu_traits.cpuid1.edx |= X86_CPUID1_EDX_FPU;
		}
		else if(emu->x87.fpu_type == X87_FPU_NONE && (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_FPU) != 0)
		{
			emu->x87.fpu_type = X87_FPU_INTEGRATED;
		}

		emu->parser->fpu_type = emu->x87.fpu_type;
		emu->parser->fpu_subtype = emu->x87.fpu_subtype;

		// TODO: other settings
		if(emu->cpu_type == X86_CPU_INTEL
			&& ((emu->cpu_traits.cpuid1.eax & 0x000F0000) == 0x00060000
				|| (emu->cpu_traits.cpuid1.eax & 0x000F0000) == 0x000F0000))
		{
			emu->cpu_traits.multibyte_nop = true;
		}

		emu->x89.initialized = false;
	}	

	for(int i = 0; i < X86_GPR_COUNT; i++)
	{
		// TODO: dx
		emu->gpr[i] = 0;
	}

	for(int i = 0; i < X86_SR_COUNT; i++)
	{
		emu->sr[i].selector = 0;
		emu->sr[i].base = 0;
		emu->sr[i].limit = 0xFFFF;
		emu->sr[i].access = x86_is_32bit_only(emu) ? 0x00409300 : 0x9300;
	}

	if(emu->cpu_type < X86_CPU_286)
	{
		emu->xip = 0x0000;
		emu->sr[X86_R_CS].selector = 0xFFFF;
	}
	else
	{
		emu->xip = 0xFFF0;
		emu->sr[X86_R_CS].selector = 0xF000;
	}

	if(emu->cpu_type < X86_CPU_286)
	{
		emu->sr[X86_R_CS].base = 0x000FFFF0;
	}
	else if(emu->cpu_type < X86_CPU_386)
	{
		emu->sr[X86_R_CS].base = 0x00FF0000;
	}
	else
	{
		emu->sr[X86_R_CS].base = 0xFFFF0000;
	}
	//emu->code_writable = true;

	emu->sr[X86_R_GDTR].selector = 0;
	emu->sr[X86_R_GDTR].base = 0;
	emu->sr[X86_R_GDTR].limit = 0xFFFF;
	emu->sr[X86_R_GDTR].access = 0x8200;

	emu->sr[X86_R_IDTR].selector = 0;
	emu->sr[X86_R_IDTR].base = 0;
	emu->sr[X86_R_IDTR].limit = 0xFFFF;
	emu->sr[X86_R_IDTR].access = 0x8200;

	emu->sr[X86_R_LDTR].selector = 0;
	emu->sr[X86_R_LDTR].base = 0;
	emu->sr[X86_R_LDTR].limit = 0xFFFF;
	emu->sr[X86_R_LDTR].access = 0x8200;

	emu->sr[X86_R_TR].selector = 0;
	emu->sr[X86_R_TR].base = 0;
	emu->sr[X86_R_TR].limit = 0xFFFF;
	emu->sr[X86_R_TR].access = 0x8200;

	emu->cpl = 0;

	if(emu->cpu_type >= X86_CPU_386)
	{
		if(emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376)
		{
			emu->cr[0] = 0x0000001F;
		}
		else if(reset)
		{
			emu->cr[0] = 0x60000000;
		}
		else
		{
			emu->cr[0] &= ~0x60000000;
		}

		if(emu->x87.fpu_type >= X87_FPU_387)
			emu->cr[0] |= 0x00000010;

		emu->cr[2] = 0;
		emu->cr[3] = 0;
		emu->cr[4] = 0;
		if(x86_is_long_mode_supported(emu) && reset)
		{
			emu->cr[8] = 0;
		}
	}
	else if(emu->cpu_type == X86_CPU_286)
	{
		emu->cr[0] = 0xFFF0;
	}

	if(emu->cpu_type >= X86_CPU_386)
	{
		emu->dr[0] = 0;
		emu->dr[1] = 0;
		emu->dr[2] = 0;
		emu->dr[3] = 0;
		emu->dr[6] = 0xFFFF0FF0;
		if(emu->cpu_type >= X86_CPU_586)
			emu->dr[7] = 0x00000400;
		else
			emu->dr[7] = 0x00000000;
	}

	emu->kernel_gs_bas = 0;

	if(reset)
	{
		memset(emu->xmm, 0, sizeof emu->xmm);

		emu->xcr0 = 1;
		emu->mxcsr = 0x00001F80;

		emu->efer = 0;

		emu->tsc = 0;

		emu->sep_sel = 0;
		emu->sep_rsp = 0;
		emu->sep_rip = 0;

		emu->star = 0;
		emu->lstar = 0;
		emu->cstar = 0;
		emu->fmask = 0;
	}

	/* FLAGS register */
	emu->cf = emu->pf = emu->af = emu->zf = emu->sf = emu->tf = emu->_if = emu->df = emu->of = 0;

	emu->ibrk_ = X86_FL_IBRK_;
	if(emu->cpu_type == X86_CPU_V25)
	{
		emu->rb = 7;
	}
	if(emu->cpu_type == X86_CPU_V55)
	{
		emu->rb = 15;
	}

	emu->md = x86_native_state_flag(emu);
	if(emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002)
	{
		emu->md_enabled = false;
		emu->full_z80_emulation = false;

		if(emu->cpu_type == X86_CPU_V20)
			emu->x80.cpu_type = X80_CPU_I80; //X80_CPU_V20;
		else if(emu->cpu_type == X86_CPU_UPD9002)
			emu->x80.cpu_type = X80_CPU_Z80; //X80_CPU_UPD9002;
		emu->x80.cpu_method = X80_CPUMETHOD_EMULATED;
	}
	else if(emu->cpu_type == X86_CPU_EXTENDED)
	{
		emu->x80.cpu_type = X80_CPU_Z80; //X80_CPU_UPD9002;
		emu->x80.cpu_method = X80_CPUMETHOD_EMULATED;
	}

	if(emu->cpu_type == X86_CPU_V25 && emu->cpu_traits.cpu_subtype == X86_CPU_V25_V25S)
	{
		for(int i = 0; i < 256; i++)
			default_opcode_translation_table[i] = i;
		emu->parser->opcode_translation_table = &default_opcode_translation_table;
	}

	if(emu->cpu_type >= X86_CPU_286)
	{
		emu->iopl = emu->nt = 0;
	}

	if(emu->cpu_type >= X86_CPU_386)
	{
		emu->rf = emu->vm = 0;
	}

	if(emu->cpu_type >= X86_CPU_486)
	{
		emu->ac = 0;
	}

	if(emu->cpu_type >= X86_CPU_586)
	{
		emu->vif = emu->vip = emu->id = 0;
	}

	emu->v33_xam &= ~X86_XAM_XA;

	for(int i = 0; i < 8; i++)
	{
		emu->k[i].q[0] = 0;
	}
	for(int i = 0; i < 4; i++)
	{
		emu->bnd[i].lower_bound = emu->bnd[i].upper_bound = 0;
	}
	emu->bndcfgs = emu->bndcfgu = emu->bndstatus = 0;

	if(reset)
	{
		for(int b = 0; b < (emu->x87.fpu_type == X87_FPU_IIT ? 4 : 1); b++)
		{
			for(int i = 0; i < 8; i++)
			{
#if _SUPPORT_FLOAT80
				emu->x87.bank[b].fpr[i].isfp = true;
				emu->x87.bank[b].fpr[i].f = 0.0;
#else
				emu->x87.bank[b].fpr[i].f.mantissa = 0;
				emu->x87.bank[b].fpr[i].f.exponent = 0;
#endif
		
			}
		}
		x87_reset(emu, reset);
		emu->x87.current_bank = 0; // IT 3c87
		emu->x87.queued_operation = X87_OP_NONE;
		emu->x87.protected_mode = false;
	}

	if(emu->cpu_type == X86_CPU_CYRIX)
	{
		emu->port22_accessed = false;

		emu->ccr[0] = 0;
		emu->ccr[1] = 0;
		emu->ccr[2] = 0;
		emu->ccr[3] = 0;
		emu->ccr[4] = 0; // TODO: MediaGX and derivatives set this to 0x85, 6x86MX to 0x80
		emu->ccr[7] = 0;

		emu->arr[0] = 0;
		emu->arr[1] = 0x000F;
		emu->arr[2] = 0;
		emu->arr[3] = 0;

		emu->smm_hdr = 0;
	}

	emu->x80.peripheral_data_length = emu->x80.peripheral_data_pointer = 0;
	emu->x80.peripheral_data = NULL;

	if(emu->cpu_type == X86_CPU_186)
	{
		emu->pcb[X86_PCB_PCR] = 0x20FF;
	}
	else if(emu->cpu_type == X86_CPU_V25)
	{
		emu->iram[0x101] = 0xFF; // PM0
		emu->iram[0x109] = 0xFF; // PM1
		emu->iram[0x111] = 0xFF; // PM2
		emu->iram[0x13B] = 0x00; // PMT
		emu->iram[0x102] = 0x00; // PMC0
		emu->iram[0x10A] = 0x00; // PMC1
		emu->iram[0x112] = 0x00; // PMC2
		emu->iram[0x190] = 0x00; // TMC0
		emu->iram[0x191] = 0x00; // TMC1
		emu->iram[0x19C] = 0x47; // TMIC0
		emu->iram[0x19D] = 0x47; // TMIC1
		emu->iram[0x19E] = 0x47; // TMIC2
		emu->iram[0x1A1] = 0x00; // DMAM0
		emu->iram[0x1A3] = 0x00; // DMAM1
		emu->iram[0x1AC] = 0x47; // DIC0
		emu->iram[0x1AD] = 0x47; // DIC1
		emu->iram[0x168] = 0x00; // SCM0
		emu->iram[0x178] = 0x00; // SCM1
		emu->iram[0x169] = 0x00; // SCC0
		emu->iram[0x179] = 0x00; // SCC1
		emu->iram[0x16A] = 0x00; // BRG0
		emu->iram[0x17A] = 0x00; // BRG1
		emu->iram[0x16B] = 0x00; // SCE0
		emu->iram[0x17B] = 0x00; // SCE1
		emu->iram[0x16C] = 0x47; // SEIC0
		emu->iram[0x17C] = 0x47; // SEIC1
		emu->iram[0x16D] = 0x47; // SRIC0
		emu->iram[0x17D] = 0x47; // SRIC1
		emu->iram[0x16E] = 0x47; // STIC0
		emu->iram[0x17E] = 0x47; // STIC1
		emu->iram[0x1EC] = 0x47; // TBIC
		emu->iram[X86_SFR_FLAG] = 0; // 0x1EA
		emu->iram[X86_SFR_IDB] = 0xFF; // 0x1FF
		emu->iram[X86_SFR_PRC] = 0x4E; // 0x1EB
		emu->iram[0x1E8] = 0xFF; // WTC (low byte)
		emu->iram[0x1E9] = 0xFF; // WTC (high byte)
		emu->iram[0x1E1] = 0xFC; // RFM
		if(reset)
			emu->iram[0x1E0] = 0x00; // STBC
		emu->iram[0x140] = 0x00; // INTM
		emu->iram[0x14C] = 0x47; // EXIC0
		emu->iram[0x14D] = 0x47; // EXIC1
		emu->iram[0x14E] = 0x47; // EXIC2
		emu->iram[0x1FC] = 0x00; // ISPR
	}
	if(emu->cpu_type == X86_CPU_V55)
	{
		x86_memory_write8(emu, 0xFFE18, 0x90); // PAC0
		x86_memory_write8(emu, 0xFFE19, 0x03); // PAC1
		x86_memory_write8(emu, 0xFFE1A, 0x40); // PAS
		x86_memory_write8(emu, 0xFFE20, 0x00); // ADM
		x86_memory_write8(emu, 0xFFEC0, 0xFF); // MK0L
		x86_memory_write8(emu, 0xFFEC1, 0xFF); // MK0H
		x86_memory_write8(emu, 0xFFEC2, 0xFF); // MK1L
		x86_memory_write8(emu, 0xFFEC3, 0xFF); // MK1H
		x86_memory_write8(emu, 0xFFEC4, 0x00); // ISPR
		x86_memory_write8(emu, 0xFFEC5, 0x80); // IMC
		x86_memory_write8(emu, 0xFFEC9, 0x43); // IC09
		x86_memory_write8(emu, 0xFFECA, 0x43); // IC10
		x86_memory_write8(emu, 0xFFECB, 0x43); // IC11
		x86_memory_write8(emu, 0xFFECC, 0x43); // IC12
		x86_memory_write8(emu, 0xFFECD, 0x43); // IC13
		x86_memory_write8(emu, 0xFFECE, 0x43); // IC14
		x86_memory_write8(emu, 0xFFED0, 0x43); // IC16
		x86_memory_write8(emu, 0xFFED1, 0x43); // IC17
		x86_memory_write8(emu, 0xFFED2, 0x43); // IC18
		x86_memory_write8(emu, 0xFFED3, 0x43); // IC19
		x86_memory_write8(emu, 0xFFED4, 0x43); // IC20
		x86_memory_write8(emu, 0xFFED5, 0x43); // IC21
		x86_memory_write8(emu, 0xFFED6, 0x43); // IC22
		x86_memory_write8(emu, 0xFFED7, 0x43); // IC23
		x86_memory_write8(emu, 0xFFED8, 0x43); // IC24
		x86_memory_write8(emu, 0xFFED9, 0x43); // IC25
		x86_memory_write8(emu, 0xFFEDA, 0x43); // IC26
		x86_memory_write8(emu, 0xFFEDB, 0x43); // IC27
		x86_memory_write8(emu, 0xFFEDC, 0x43); // IC28
		x86_memory_write8(emu, 0xFFEDD, 0x43); // IC29
		x86_memory_write8(emu, 0xFFEDE, 0x43); // IC30
		x86_memory_write8(emu, 0xFFEDF, 0x43); // IC31
		x86_memory_write8(emu, 0xFFEE0, 0x43); // IC32
		x86_memory_write8(emu, 0xFFEE4, 0x43); // IC36
		x86_memory_write8(emu, 0xFFEE5, 0x43); // IC37
		x86_memory_write8(emu, 0xFFF0C, 0x00); // PRDC
		x86_memory_write8(emu, 0xFFF10, 0xFF); // PM0
		x86_memory_write8(emu, 0xFFF12, 0xFF); // PM2
		x86_memory_write8(emu, 0xFFF13, 0xFF); // PM3
		x86_memory_write8(emu, 0xFFF14, 0xFF); // PM4
		x86_memory_write8(emu, 0xFFF15, 0xFF); // PM5
		x86_memory_write8(emu, 0xFFF17, 0xFF); // PM7
		x86_memory_write8(emu, 0xFFF18, 0xFF); // PM8
		x86_memory_write8(emu, 0xFFF22, 0x00); // PMC2
		x86_memory_write8(emu, 0xFFF23, 0x00); // PMC3
		x86_memory_write8(emu, 0xFFF24, 0x00); // PMC4
		x86_memory_write8(emu, 0xFFF25, 0x00); // PMC5
		x86_memory_write8(emu, 0xFFF27, 0x00); // PMC7
		x86_memory_write8(emu, 0xFFF28, 0x00); // PMC8
		x86_memory_write8(emu, 0xFFF2C, 0x40); // RTPC
		x86_memory_write8(emu, 0xFFF30, 0x00); // TMC0
		x86_memory_write8(emu, 0xFFF31, 0x00); // TMC1
		x86_memory_write8(emu, 0xFFF32, 0x00); // TOC0
		x86_memory_write8(emu, 0xFFF33, 0x00); // TOC1
		x86_memory_write8(emu, 0xFFF34, 0x00); // INTM0
		x86_memory_write8(emu, 0xFFF35, 0x00); // INTM1
		x86_memory_write8(emu, 0xFFF40, 0x00); // TM0
		x86_memory_write16(emu, 0xFFF42, 0x0000); // TM1
		x86_memory_write16(emu, 0xFFF44, 0x0000); // TM2
		x86_memory_write16(emu, 0xFFF46, 0x0000); // TM3
		x86_memory_write8(emu, 0xFFF60, 0x00); // WDM
		x86_memory_write8(emu, 0xFFF6C, 0x00); // PWM
		x86_memory_write8(emu, 0xFFF6D, 0x00); // PWMC
		x86_memory_write8(emu, 0xFFF72, 0x00); // PRS0
		x86_memory_write8(emu, 0xFFF73, 0x00); // UARTM0/CSIM0
		x86_memory_write8(emu, 0xFFF74, 0x00); // UARTS0/SBIC0
		x86_memory_write8(emu, 0xFFF7A, 0x00); // PRS1
		x86_memory_write8(emu, 0xFFF7B, 0x00); // UARTM1/CSIM1
		x86_memory_write8(emu, 0xFFF7C, 0x00); // UARTS1/SBIC1
		x86_memory_write8(emu, 0xFFF7F, 0x00); // ASP
		x86_memory_write8(emu, 0xFFF9C, 0xE0); // DMAM0
		x86_memory_write8(emu, 0xFFF9D, 0x00); // DMAC0
		x86_memory_write8(emu, 0xFFF9E, 0x00); // DMAS
		x86_memory_write8(emu, 0xFFFBC, 0xE0); // DMAM1
		x86_memory_write8(emu, 0xFFFBD, 0x00); // DMAC1
		x86_memory_write16(emu, 0xFFFEC, 0xFFFF); // STMC
		x86_memory_write8(emu, 0xFFFE8, 0xEA); // PWC0
		x86_memory_write8(emu, 0xFFFE9, 0xAA); // PWC1
		x86_memory_write8(emu, 0xFFFEA, 0xFC); // MBC
		x86_memory_write8(emu, 0xFFFEB, 0x77); // RFM
		if(reset)
			x86_memory_write8(emu, 0xFFFEE, 0x00); // STBC
		x86_memory_write8(emu, 0xFFFEF, 0xEE); // PRC
	}

	if(emu->cpu_type == X86_CPU_V25 || emu->cpu_type == X86_CPU_V55)
	{
		x86_store_register_bank(emu);
	}
	// TODO: clear anything else

	for(int i = 0; i < 8; i++)
	{
		emu->k[i].q[0] = 0;
	}
	for(int i = 0; i < 4; i++)
	{
		emu->bnd[i].lower_bound = emu->bnd[i].upper_bound = 0;
	}
	emu->bndcfgs = emu->bndcfgu = emu->bndstatus = 0;

	emu->halted = false;
}

static void x86_debug64(FILE * file, x86_state_t * emu)
{
	fprintf(file, "RAX=%016"PRIX64",RCX=%016"PRIX64",RDX=%016"PRIX64",RBX=%016"PRIX64"\n",
		emu->gpr[X86_R_AX], emu->gpr[X86_R_CX], emu->gpr[X86_R_DX], emu->gpr[X86_R_BX]);
	fprintf(file, "RSP=%016"PRIX64",RBP=%016"PRIX64",RSI=%016"PRIX64",RDI=%016"PRIX64"\n",
		emu->gpr[X86_R_SP], emu->gpr[X86_R_BP], emu->gpr[X86_R_SI], emu->gpr[X86_R_DI]);
	fprintf(file, "R8 =%016"PRIX64",R9 =%016"PRIX64",R10=%016"PRIX64",R11=%016"PRIX64"\n",
		emu->gpr[8], emu->gpr[9], emu->gpr[10], emu->gpr[11]);
	fprintf(file, "R12=%016"PRIX64",R13=%016"PRIX64",R14=%016"PRIX64",R15=%016"PRIX64"\n",
		emu->gpr[12], emu->gpr[13], emu->gpr[14], emu->gpr[15]);
	if(false) // TODO: R16-R31 (CPUID.7.1.EDX.21/APX_F)
	{
		fprintf(file, "R16=%016"PRIX64",R17=%016"PRIX64",R18=%016"PRIX64",R19=%016"PRIX64"\n",
			emu->gpr[16], emu->gpr[17], emu->gpr[18], emu->gpr[19]);
		fprintf(file, "R20=%016"PRIX64",R21=%016"PRIX64",R22=%016"PRIX64",R23=%016"PRIX64"\n",
			emu->gpr[20], emu->gpr[21], emu->gpr[22], emu->gpr[23]);
		fprintf(file, "R24=%016"PRIX64",R25=%016"PRIX64",R26=%016"PRIX64",R27=%016"PRIX64"\n",
			emu->gpr[24], emu->gpr[25], emu->gpr[26], emu->gpr[27]);
		fprintf(file, "R28=%016"PRIX64",R29=%016"PRIX64",R30=%016"PRIX64",R31=%016"PRIX64"\n",
			emu->gpr[28], emu->gpr[29], emu->gpr[30], emu->gpr[31]);
	}
	fprintf(file, "RIP=%016"PRIX64",RFLAGS=%016"PRIX64"\n",
		emu->xip, x86_flags_get64(emu));
	fprintf(file, "CF=%d,PF=%d,AF=%d,ZF=%d,SF=%d,TF=%d,IF=%d,DF=%d,OF=%d,IOPL=%d,NT=%d,RF=%d,VM=%d,AC=%d,VIF=%d,VIP=%d,ID=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->iopl, emu->nt!=0,
		emu->rf!=0, emu->vm!=0, emu->ac!=0, emu->vif!=0, emu->vip!=0, emu->id!=0);
	fprintf(file, "ES  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base, emu->sr[X86_R_ES].limit, emu->sr[X86_R_ES].access >> 8);
	fprintf(file, "CS  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base, emu->sr[X86_R_CS].limit, emu->sr[X86_R_CS].access >> 8);
	fprintf(file, "SS  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base, emu->sr[X86_R_SS].limit, emu->sr[X86_R_SS].access >> 8);
	fprintf(file, "DS  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base, emu->sr[X86_R_DS].limit, emu->sr[X86_R_DS].access >> 8);
	fprintf(file, "FS  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_FS].selector, emu->sr[X86_R_FS].base, emu->sr[X86_R_FS].limit, emu->sr[X86_R_FS].access >> 8);
	fprintf(file, "GS  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_GS].selector, emu->sr[X86_R_GS].base, emu->sr[X86_R_GS].limit, emu->sr[X86_R_GS].access >> 8);
	fprintf(file, "GDTR     :base=%016"PRIX64",limit=%08"PRIX32"\n",
		emu->sr[X86_R_GDTR].base, emu->sr[X86_R_GDTR].limit);
	fprintf(file, "IDTR     :base=%016"PRIX64",limit=%08"PRIX32"\n",
		emu->sr[X86_R_IDTR].base, emu->sr[X86_R_IDTR].limit);
	fprintf(file, "LDTR=%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_LDTR].selector, emu->sr[X86_R_LDTR].base, emu->sr[X86_R_LDTR].limit, emu->sr[X86_R_LDTR].access >> 8);
	fprintf(file, "TR  =%04"PRIX16":base=%016"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_TR].selector, emu->sr[X86_R_TR].base, emu->sr[X86_R_TR].limit, emu->sr[X86_R_TR].access >> 8);
	fprintf(file, "CR0=%016"PRIX64",CR2=%016"PRIX64",CR3=%016"PRIX64",CR4=%016"PRIX64"\n", emu->cr[0], emu->cr[2], emu->cr[3], emu->cr[4]);
	fprintf(file, "CR8=%016"PRIX64",XCR0=%016"PRIX64",EFER=%016"PRIX64"\n", emu->cr[8], emu->xcr0, emu->efer);
	fprintf(file, "DR0=%016"PRIX64",DR1=%016"PRIX64",DR2=%016"PRIX64",DR3=%016"PRIX64"\n", emu->dr[0], emu->dr[1], emu->dr[2], emu->dr[3]);
	fprintf(file, "DR6=%016"PRIX64",DR7=%016"PRIX64"\n", emu->dr[6], emu->dr[7]);
	fprintf(file, "TR3=%08"PRIX32",TR4=%08"PRIX32",TR5=%08"PRIX32",TR6=%08"PRIX32",TR7=%08"PRIX32"\n", emu->tr386[3], emu->tr386[4], emu->tr386[5], emu->tr386[6], emu->tr386[7]);
	/* TODO: SSE registers */
	/* TODO: model specific registers */
	/* TODO: mask, bound, shadow registers */
}

static void x86_debug32(FILE * file, x86_state_t * emu)
{
	fprintf(file, "EAX=%08"PRIX32",ECX=%08"PRIX32",EDX=%08"PRIX32",EBX=%08"PRIX32"\n",
		(uint32_t)emu->gpr[X86_R_AX], (uint32_t)emu->gpr[X86_R_CX], (uint32_t)emu->gpr[X86_R_DX], (uint32_t)emu->gpr[X86_R_BX]);
	fprintf(file, "ESP=%08"PRIX32",EBP=%08"PRIX32",ESI=%08"PRIX32",EDI=%08"PRIX32"\n",
		(uint32_t)emu->gpr[X86_R_SP], (uint32_t)emu->gpr[X86_R_BP], (uint32_t)emu->gpr[X86_R_SI], (uint32_t)emu->gpr[X86_R_DI]);
	fprintf(file, "EIP=%08"PRIX32",EFLAGS=%08"PRIX32"\n",
		(uint32_t)emu->xip, x86_flags_get32(emu));
	fprintf(file, "CF=%d,PF=%d,AF=%d,ZF=%d,SF=%d,TF=%d,IF=%d,DF=%d,OF=%d,IOPL=%d,NT=%d,RF=%d,VM=%d,AC=%d,VIF=%d,VIP=%d,ID=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->iopl, emu->nt!=0,
		emu->rf!=0, emu->vm!=0, emu->ac!=0, emu->vif!=0, emu->vip!=0, emu->id!=0);
	fprintf(file, "ES  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base, emu->sr[X86_R_ES].limit, emu->sr[X86_R_ES].access >> 8);
	fprintf(file, "CS  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base, emu->sr[X86_R_CS].limit, emu->sr[X86_R_CS].access >> 8);
	fprintf(file, "SS  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base, emu->sr[X86_R_SS].limit, emu->sr[X86_R_SS].access >> 8);
	fprintf(file, "DS  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base, emu->sr[X86_R_DS].limit, emu->sr[X86_R_DS].access >> 8);
	fprintf(file, "FS  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_FS].selector, emu->sr[X86_R_FS].base, emu->sr[X86_R_FS].limit, emu->sr[X86_R_FS].access >> 8);
	fprintf(file, "GS  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_GS].selector, emu->sr[X86_R_GS].base, emu->sr[X86_R_GS].limit, emu->sr[X86_R_GS].access >> 8);
	fprintf(file, "GDTR     :base=%08"PRIX64",limit=%08"PRIX32"\n",
		emu->sr[X86_R_GDTR].base, emu->sr[X86_R_GDTR].limit);
	fprintf(file, "IDTR     :base=%08"PRIX64",limit=%08"PRIX32"\n",
		emu->sr[X86_R_IDTR].base, emu->sr[X86_R_IDTR].limit);
	fprintf(file, "LDTR=%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_LDTR].selector, emu->sr[X86_R_LDTR].base, emu->sr[X86_R_LDTR].limit, emu->sr[X86_R_LDTR].access >> 8);
	fprintf(file, "TR  =%04"PRIX16":base=%08"PRIX64",limit=%08"PRIX32",access=%04"PRIX32"\n",
		emu->sr[X86_R_TR].selector, emu->sr[X86_R_TR].base, emu->sr[X86_R_TR].limit, emu->sr[X86_R_TR].access >> 8);
	fprintf(file, "CR0=%08"PRIX64",CR2=%08"PRIX64",CR3=%08"PRIX64",CR4=%08"PRIX64"\n", emu->cr[0], emu->cr[2], emu->cr[3], emu->cr[4]);
	fprintf(file, "CR8=%08"PRIX64",XCR0=%08"PRIX64"\n", emu->cr[8], emu->xcr0);
	fprintf(file, "DR0=%08"PRIX64",DR1=%08"PRIX64",DR2=%08"PRIX64",DR3=%08"PRIX64"\n", emu->dr[0], emu->dr[1], emu->dr[2], emu->dr[3]);
	fprintf(file, "DR6=%08"PRIX64",DR7=%08"PRIX64"\n", emu->dr[6], emu->dr[7]);
	fprintf(file, "TR3=%08"PRIX32",TR4=%08"PRIX32",TR5=%08"PRIX32",TR6=%08"PRIX32",TR7=%08"PRIX32"\n", emu->tr386[3], emu->tr386[4], emu->tr386[5], emu->tr386[6], emu->tr386[7]);
	/* TODO: SSE registers */
	/* TODO: model specific registers */
	/* TODO: mask, bound, shadow registers */
}

static void x86_debug_286(FILE * file, x86_state_t * emu)
{
	fprintf(file, "AX=%04"PRIX16",CX=%04"PRIX16",DX=%04"PRIX16",BX=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",SI=%04"PRIX16",DI=%04"PRIX16"\n",
		(uint16_t)emu->gpr[X86_R_AX], (uint16_t)emu->gpr[X86_R_CX], (uint16_t)emu->gpr[X86_R_DX], (uint16_t)emu->gpr[X86_R_BX],
		(uint16_t)emu->gpr[X86_R_SP], (uint16_t)emu->gpr[X86_R_BP], (uint16_t)emu->gpr[X86_R_SI], (uint16_t)emu->gpr[X86_R_DI]);
	fprintf(file, "IP=%04"PRIX16",FLAGS=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CF=%d,PF=%d,AF=%d,ZF=%d,SF=%d,TF=%d,IF=%d,DF=%d,OF=%d,IOPL=%d,NT=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->iopl, emu->nt!=0);
	fprintf(file, "ES  =%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base, emu->sr[X86_R_ES].limit, emu->sr[X86_R_ES].access >> 8);
	fprintf(file, "CS  =%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base, emu->sr[X86_R_CS].limit, emu->sr[X86_R_CS].access >> 8);
	fprintf(file, "SS  =%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base, emu->sr[X86_R_SS].limit, emu->sr[X86_R_SS].access >> 8);
	fprintf(file, "DS  =%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base, emu->sr[X86_R_DS].limit, emu->sr[X86_R_DS].access >> 8);
	fprintf(file, "GDTR     :base=%06"PRIX64",limit=%04"PRIX32"\n",
		emu->sr[X86_R_GDTR].base, emu->sr[X86_R_GDTR].limit);
	fprintf(file, "IDTR     :base=%06"PRIX64",limit=%04"PRIX32"\n",
		emu->sr[X86_R_IDTR].base, emu->sr[X86_R_IDTR].limit);
	fprintf(file, "LDTR=%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_LDTR].selector, emu->sr[X86_R_LDTR].base, emu->sr[X86_R_LDTR].limit, emu->sr[X86_R_LDTR].access >> 8);
	fprintf(file, "TR  =%04"PRIX16":base=%06"PRIX64",limit=%04"PRIX32",access=%02"PRIX32"\n",
		emu->sr[X86_R_TR].selector, emu->sr[X86_R_TR].base, emu->sr[X86_R_TR].limit, emu->sr[X86_R_TR].access >> 8);
	fprintf(file, "MSW =%04"PRIX16"\n", (uint16_t)emu->cr[0]);
}

static void x86_debug16(FILE * file, x86_state_t * emu)
{
	fprintf(file, "AX=%04"PRIX16",CX=%04"PRIX16",DX=%04"PRIX16",BX=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",SI=%04"PRIX16",DI=%04"PRIX16"\n",
		(uint16_t)emu->gpr[X86_R_AX], (uint16_t)emu->gpr[X86_R_CX], (uint16_t)emu->gpr[X86_R_DX], (uint16_t)emu->gpr[X86_R_BX],
		(uint16_t)emu->gpr[X86_R_SP], (uint16_t)emu->gpr[X86_R_BP], (uint16_t)emu->gpr[X86_R_SI], (uint16_t)emu->gpr[X86_R_DI]);
	fprintf(file, "IP=%04"PRIX16",FLAGS=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CF=%d,PF=%d,AF=%d,ZF=%d,SF=%d,TF=%d,IF=%d,DF=%d,OF=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0);
	fprintf(file, "ES=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base);
	fprintf(file, "CS=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base);
	fprintf(file, "SS=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base);
	fprintf(file, "DS=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base);
}

static void x86_debug_nec(FILE * file, x86_state_t * emu)
{
	fprintf(file, "AW=%04"PRIX16",CW=%04"PRIX16",DW=%04"PRIX16",BW=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",IX=%04"PRIX16",IY=%04"PRIX16"\n",
		(uint16_t)emu->gpr[X86_R_AX], (uint16_t)emu->gpr[X86_R_CX], (uint16_t)emu->gpr[X86_R_DX], (uint16_t)emu->gpr[X86_R_BX],
		(uint16_t)emu->gpr[X86_R_SP], (uint16_t)emu->gpr[X86_R_BP], (uint16_t)emu->gpr[X86_R_SI], (uint16_t)emu->gpr[X86_R_DI]);
	fprintf(file, "PC=%04"PRIX16",PSW=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CY=%d,P=%d,AC=%d,Z=%d,S=%d,BRK=%d,IE=%d,DIR=%d,V=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0);
	fprintf(file, "DS1=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base);
	fprintf(file, "PS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base);
	fprintf(file, "SS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base);
	fprintf(file, "DS0=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base);
}

static void x86_debug_v20(FILE * file, x86_state_t * emu)
{
	x86_store_x80_registers(emu);
	fprintf(file, "AW=%04"PRIX16",CW=%04"PRIX16",DW=%04"PRIX16",BW=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",IX=%04"PRIX16",IY=%04"PRIX16"\n",
		(uint16_t)emu->gpr[X86_R_AX], (uint16_t)emu->gpr[X86_R_CX], (uint16_t)emu->gpr[X86_R_DX], (uint16_t)emu->gpr[X86_R_BX],
		(uint16_t)emu->gpr[X86_R_SP], (uint16_t)emu->gpr[X86_R_BP], (uint16_t)emu->gpr[X86_R_SI], (uint16_t)emu->gpr[X86_R_DI]);
	fprintf(file, "PC=%04"PRIX16",PSW=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CY=%d,P=%d,AC=%d,Z=%d,S=%d,BRK=%d,IE=%d,DIR=%d,V=%d,MD=%d\n",
		emu->cf!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->md!=0);
	fprintf(file, "DS1=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base);
	fprintf(file, "PS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base);
	fprintf(file, "SS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base);
	fprintf(file, "DS0=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base);
}

static void x86_debug_upd9002(FILE * file, x86_state_t * emu)
{
	x86_store_x80_registers(emu);
	fprintf(file, "AW0=%04"PRIX16",CW0=%04"PRIX16",DW0=%04"PRIX16",BW0=%04"PRIX16",SP =%04"PRIX16",BP =%04"PRIX16",IX =%04"PRIX16",IY =%04"PRIX16"\n",
		(uint16_t)emu->gpr[X86_R_AX], (uint16_t)emu->gpr[X86_R_CX], (uint16_t)emu->gpr[X86_R_DX], (uint16_t)emu->gpr[X86_R_BX],
		(uint16_t)emu->gpr[X86_R_SP], (uint16_t)emu->gpr[X86_R_BP], (uint16_t)emu->gpr[X86_R_SI], (uint16_t)emu->gpr[X86_R_DI]);
	fprintf(file, "AW1=  %02"PRIX16",CW1=%04"PRIX16",DW1=%04"PRIX16",BW1=%04"PRIX16"\n",
		(uint8_t)emu->x80.bank[1 - emu->x80.af_bank].af >> 8,
		(uint16_t)emu->x80.bank[1 - emu->x80.main_bank].bc,
		(uint16_t)emu->x80.bank[1 - emu->x80.main_bank].de,
		(uint16_t)emu->x80.bank[1 - emu->x80.main_bank].hl);
	fprintf(file, "PC=%04"PRIX16",PSW0=%04"PRIX16",PSW1=  %02"PRIX16",I=%02"PRIX16",R=%02"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu), (uint8_t)(emu->x80.bank[1].af & 0xFF), emu->x80.i, emu->x80.r);
	fprintf(file, "CY0=%d,CY1=%d,NF0=%d,NF1=%d,P0=%d,P1=%d,AC0=%d,AC1=%d,Z0=%d,Z1=%d,S0=%d,S1=%d,BRK=%d,IE1=%d,IE2=%d,DIR=%d,V=%d,MD=%d,AF bank:%d,main bank:%d\n",
		emu->cf!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&X86_FL_CF)!=0,
		(emu->z80_flags&2)!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&2)!=0,
		emu->pf!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&X86_FL_PF)!=0,
		emu->af!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&X86_FL_AF)!=0,
		emu->zf!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&X86_FL_ZF)!=0,
		emu->sf!=0, (emu->x80.bank[1 - emu->x80.af_bank].af&X86_FL_SF)!=0,
		emu->tf!=0,
		emu->_if!=0, emu->x80.iff2,
		emu->df!=0, emu->of!=0, emu->md!=0, emu->x80.af_bank, emu->x80.main_bank);
	fprintf(file, "DS1=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_ES].selector, emu->sr[X86_R_ES].base);
	fprintf(file, "PS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_CS].selector, emu->sr[X86_R_CS].base);
	fprintf(file, "SS =%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_SS].selector, emu->sr[X86_R_SS].base);
	fprintf(file, "DS0=%04"PRIX16":base=%05"PRIX64"\n",
		emu->sr[X86_R_DS].selector, emu->sr[X86_R_DS].base);
}

static void x86_debug_v25(FILE * file, x86_state_t * emu)
{
	x86_store_register_bank(emu);
	for(unsigned i = 0; i < 8; i++)
	{
		fprintf(file, "%c bank #%d: AW=%04"PRIX16",CW=%04"PRIX16",DW=%04"PRIX16",BW=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",IX=%04"PRIX16",IY=%04"PRIX16"\n",
			i == emu->rb ? '*' : ' ',
			i,
			le16toh(emu->bank[i].w[X86_BANK_AW]),
			le16toh(emu->bank[i].w[X86_BANK_CW]),
			le16toh(emu->bank[i].w[X86_BANK_DW]),
			le16toh(emu->bank[i].w[X86_BANK_BW]),
			le16toh(emu->bank[i].w[X86_BANK_SP]),
			le16toh(emu->bank[i].w[X86_BANK_BP]),
			le16toh(emu->bank[i].w[X86_BANK_IX]),
			le16toh(emu->bank[i].w[X86_BANK_IY]));
		fprintf(file, "%c bank #%d: DS1=%04"PRIX16":base=%05"PRIX32",", i == emu->rb ? '*' : ' ', i,
		                                                                   emu->bank[i].w[X86_BANK_DS1], (uint32_t)emu->bank[i].w[X86_BANK_DS1] << 4);
		fprintf(file,              "PS =%04"PRIX16":base=%05"PRIX32",",    emu->bank[i].w[X86_BANK_PS],  (uint32_t)emu->bank[i].w[X86_BANK_PS] << 4);
		fprintf(file,              "SS =%04"PRIX16":base=%05"PRIX32",",    emu->bank[i].w[X86_BANK_SS],  (uint32_t)emu->bank[i].w[X86_BANK_SS] << 4);
		fprintf(file,              "DS0=%04"PRIX16":base=%05"PRIX32"\n",   emu->bank[i].w[X86_BANK_DS0], (uint32_t)emu->bank[i].w[X86_BANK_DS0] << 4);
	}
	fprintf(file, "PC=%04"PRIX16",PSW=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CY=%d,^BRK=%d,P=%d,AC=%d,Z=%d,S=%d,BRK=%d,IE=%d,DIR=%d,V=%d,RB=%d\n",
		emu->cf!=0, emu->ibrk_!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->rb);
}

static void x86_debug_v55(FILE * file, x86_state_t * emu)
{
	x86_store_register_bank(emu);
	for(unsigned i = 0; i < 16; i++)
	{
		fprintf(file, "%c bank #%d: AW=%04"PRIX16",CW=%04"PRIX16",DW=%04"PRIX16",BW=%04"PRIX16",SP=%04"PRIX16",BP=%04"PRIX16",IX=%04"PRIX16",IY=%04"PRIX16"\n",
			i == emu->rb ? '*' : ' ',
			i,
			le16toh(emu->bank[i].w[X86_BANK_AW]),
			le16toh(emu->bank[i].w[X86_BANK_CW]),
			le16toh(emu->bank[i].w[X86_BANK_DW]),
			le16toh(emu->bank[i].w[X86_BANK_BW]),
			le16toh(emu->bank[i].w[X86_BANK_SP]),
			le16toh(emu->bank[i].w[X86_BANK_BP]),
			le16toh(emu->bank[i].w[X86_BANK_IX]),
			le16toh(emu->bank[i].w[X86_BANK_IY]));
		fprintf(file, "%c bank #%d: DS1=%04"PRIX16":base=%05"PRIX32",", i == emu->rb ? '*' : ' ', i,
		                                                                   emu->bank[i].w[X86_BANK_DS1], (uint32_t)emu->bank[i].w[X86_BANK_DS1] << 4);
		fprintf(file,              "PS =%04"PRIX16":base=%05"PRIX32",",    emu->bank[i].w[X86_BANK_PS],  (uint32_t)emu->bank[i].w[X86_BANK_PS] << 4);
		fprintf(file,              "SS =%04"PRIX16":base=%05"PRIX32",",    emu->bank[i].w[X86_BANK_SS],  (uint32_t)emu->bank[i].w[X86_BANK_SS] << 4);
		fprintf(file,              "DS0=%04"PRIX16":base=%05"PRIX32",",    emu->bank[i].w[X86_BANK_DS0], (uint32_t)emu->bank[i].w[X86_BANK_DS0] << 4);
		fprintf(file,              "DS3=%04"PRIX16":base=%06"PRIX32",",    emu->bank[i].w[X86_BANK_DS3], (uint32_t)emu->bank[i].w[X86_BANK_DS3] << 8);
		fprintf(file,              "DS2=%04"PRIX16":base=%06"PRIX32"\n",   emu->bank[i].w[X86_BANK_DS2], (uint32_t)emu->bank[i].w[X86_BANK_DS2] << 8);
	}
	fprintf(file, "PC=%04"PRIX16",PSW=%04"PRIX16"\n",
		(uint16_t)emu->xip, x86_flags_get16(emu));
	fprintf(file, "CY=%d,^BRK=%d,P=%d,AC=%d,Z=%d,S=%d,BRK=%d,IE=%d,DIR=%d,V=%d,RB=%d\n",
		emu->cf!=0, emu->ibrk_!=0, emu->pf!=0, emu->af!=0, emu->zf!=0, emu->sf!=0, emu->tf!=0, emu->_if!=0, emu->df!=0, emu->of!=0, emu->rb);
}

static void x87_debug(FILE * file, x86_state_t * emu)
{
	if(emu->x87.fpu_type != X87_FPU_287)
		fprintf(file, "[FPU]\n");
	else if(emu->x87.protected_mode)
		fprintf(file, "[FPU]\tProtected mode\n");
	else
		fprintf(file, "[FPU]\tReal mode\n");
	for(int i = 0; i < 8; i++)
	{
		uint64_t fraction;
		uint16_t exponent;
		bool sign;
		int number = x87_register_number(emu, i);
		float80_t fpr = x87_register_get80(emu, i);
		x87_convert_from_float80(fpr, &fraction, &exponent, &sign);
		// TODO: isfp
		fprintf(file, "FPR%d/ST(%d)=%Le:%d %04X %016"PRIX64",tag=%d\n", number, i, fpr, sign, exponent, fraction, x87_tag_get(emu, number));
	}
	fprintf(file, "CW=%04X,SW=%04X,TW=%04X\n", emu->x87.cw, emu->x87.sw, emu->x87.tw);
	switch(emu->x87.fpu_type)
	{
	case X87_FPU_8087:
		fprintf(file, "FOP=%04X,         FIP=%04X,         FDP=%04X\n", emu->x87.fop,           (uint16_t)emu->x87.fip,           (uint16_t)emu->x87.fdp);
		break;
	case X87_FPU_287:
		fprintf(file, "FOP=%04X,FCS=%04X,FIP=%04X,FDS=%04X,FDP=%04X\n", emu->x87.fop, emu->x87.fcs, (uint16_t)emu->x87.fip, emu->x87.fds, (uint16_t)emu->x87.fdp);
		break;
	default:
		if(!x86_is_long_mode_supported(emu))
			fprintf(file, "FOP=%04X,FCS=%04X,FIP=%08"PRIX32",FDS=%04X,FDP=%08"PRIX32"\n", emu->x87.fop, emu->x87.fcs, (uint32_t)emu->x87.fip, emu->x87.fds, (uint32_t)emu->x87.fdp);
		else
			fprintf(file, "FOP=%04X,FCS=%04X,FIP=%016"PRIX64",FDS=%04X,FDP=%016"PRIX64"\n", emu->x87.fop, emu->x87.fcs, emu->x87.fip, emu->x87.fds, emu->x87.fdp);
		break;
	}
}

static void x89_debug(FILE * file, x86_state_t * emu)
{
	fprintf(file, "[IOP]\n");
	for(int ch = 0; ch < 2; ch++)
	{
		fprintf(file, "%d.GA=%d:%05X\t%d.GB=%d:%05X\t%d.GC=%d:%05X\t%d.BC=   %04X\n",
			ch, emu->x89.channel[ch].r[X89_R_GA].tag, emu->x89.channel[ch].r[X89_R_GA].address,
			ch, emu->x89.channel[ch].r[X89_R_GB].tag, emu->x89.channel[ch].r[X89_R_GB].address,
			ch, emu->x89.channel[ch].r[X89_R_GC].tag, emu->x89.channel[ch].r[X89_R_GC].address,
			ch,                                       emu->x89.channel[ch].r[X89_R_BC].address & 0xFFFF);
		fprintf(file, "%d.TP=%d:%05X\t%d.IX=   %04X\t%d.CC=   %04X\t%d.MC=   %04X\n",
			ch, emu->x89.channel[ch].r[X89_R_TP].tag, emu->x89.channel[ch].r[X89_R_TP].address,
			ch,                                       emu->x89.channel[ch].r[X89_R_IX].address & 0xFFFF,
			ch,                                       emu->x89.channel[ch].r[X89_R_CC].address & 0xFFFF,
			ch,                                       emu->x89.channel[ch].r[X89_R_MC].address & 0xFFFF);
		fprintf(file, "%d.PP=  %05X\t%d.PSW=    %02X\n",
			ch, emu->x89.channel[ch].pp,
			ch, emu->x89.channel[ch].psw);
	}
	fprintf(file, "  CP=  %05X\n", emu->x89.cp);
}

void x86_debug(FILE * file, x86_state_t * emu)
{
	fprintf(file, "[CPU]\t");
	if(x86_is_long_mode(emu))
	{
		if(x86_is_emulation_mode(emu))
		{
			if(!x86_is_z80(emu))
				fprintf(file, "Long 8080 emulation mode"); // extension
			else
				fprintf(file, "Long Z80 emulation mode"); // extension
		}
		else if(x86_is_virtual_8086_mode(emu))
			fprintf(file, "Long virtual 8086 mode"); // extension
		else
			fprintf(file, "Long mode");
	}
	else if(x86_is_protected_mode(emu))
	{
		if(x86_is_emulation_mode(emu))
		{
			if(!x86_is_z80(emu))
				fprintf(file, "Protected 8080 emulation mode"); // extension
			else
				fprintf(file, "Protected Z80 emulation mode"); // extension
		}
		else if(x86_is_virtual_8086_mode(emu))
			fprintf(file, "Virtual 8086 mode");
		else
			fprintf(file, "Protected mode");
	}
	else if(!x86_is_emulation_mode(emu))
	{
		if(x86_is_nec(emu))
			fprintf(file, "Native mode");
		else
			fprintf(file, "Real mode");
	}
	else
	{
		if(!x86_is_z80(emu))
			fprintf(file, "8080 emulation mode");
		else if(emu->full_z80_emulation)
			fprintf(file, "Full Z80 emulation mode");
		else
			fprintf(file, "Z80 emulation mode");
	}

	switch(x86_get_code_size(emu))
	{
	case SIZE_8BIT:
	default:
		fprintf(file, "\n");
		break;
	case SIZE_16BIT:
		fprintf(file, ", 16-bit\n");
		break;
	case SIZE_32BIT:
		fprintf(file, ", 32-bit\n");
		break;
	case SIZE_64BIT:
		fprintf(file, ", 64-bit\n");
		break;
	}

	switch(emu->cpu_type)
	{
	case X86_CPU_V20:
		x86_debug_v20(file, emu);
		break;
	case X86_CPU_UPD9002:
		x86_debug_upd9002(file, emu);
		break;
	case X86_CPU_V60:
	case X86_CPU_V33:
		x86_debug_nec(file, emu);
		break;
	case X86_CPU_V25:
		x86_debug_v25(file, emu);
		break;
	case X86_CPU_V55:
		x86_debug_v55(file, emu);
		break;
	case X86_CPU_8086:
	case X86_CPU_186:
		x86_debug16(file, emu);
		break;
	case X86_CPU_286:
		x86_debug_286(file, emu);
		break;
//	case X86_CPU_EXTENDED:
//		x86_debug_extended(file, emu);
//		break;
	default:
		if(x86_is_long_mode_supported(emu))
		{
			x86_debug64(file, emu);
		}
		else
		{
			x86_debug32(file, emu);
		}
		break;
	}
	if(emu->x87.fpu_type != X87_FPU_NONE)
	{
		x87_debug(file, emu);
	}
	if(emu->x89.present)
	{
		x89_debug(file, emu);
	}
}

void x80_debug(FILE * file, x80_state_t * emu)
{
	switch(emu->cpu_type)
	{
	case X80_CPU_I80:
		fprintf(file, "PSW=%04"PRIX16",B  =%04"PRIX16",D  =%04"PRIX16",H  =%04"PRIX16",SP =%04"PRIX16"\n",
			emu->bank[emu->af_bank].af, emu->bank[emu->main_bank].bc, emu->bank[emu->main_bank].de, emu->bank[emu->main_bank].hl, emu->sp);
		fprintf(file, "PC=%04"PRIX16",C=%d,P=%d,AC=%d,Z=%d,S=%d,IE=%d\n",
			emu->pc,
			(emu->bank[emu->af_bank].af&X86_FL_CF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_PF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_AF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_ZF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_SF)!=0,
			emu->iff1);
		break;
	case X80_CPU_I85:
		fprintf(file, "PSW=%04"PRIX16",B  =%04"PRIX16",D  =%04"PRIX16",H  =%04"PRIX16",SP =%04"PRIX16"\n",
			emu->bank[emu->af_bank].af, emu->bank[emu->main_bank].bc, emu->bank[emu->main_bank].de, emu->bank[emu->main_bank].hl, emu->sp);
		fprintf(file, "PC=%04"PRIX16",C=%d,(V)=%d,P=%d,AC=%d,(K)=%d,Z=%d,S=%d,IE=%d\n",
			emu->pc,
			(emu->bank[emu->af_bank].af&X86_FL_CF)!=0,
			(emu->bank[emu->af_bank].af&0x02)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_PF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_AF)!=0,
			(emu->bank[emu->af_bank].af&0x20)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_ZF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_SF)!=0,
			emu->iff1);
		break;
	case X80_CPU_Z80:
		fprintf(file, "AF =%04"PRIX16",BC =%04"PRIX16",DE =%04"PRIX16",HL =%04"PRIX16",SP =%04"PRIX16",IX =%04"PRIX16",IY =%04"PRIX16"\n",
			emu->bank[emu->af_bank].af, emu->bank[emu->main_bank].bc, emu->bank[emu->main_bank].de, emu->bank[emu->main_bank].hl,
			emu->sp, emu->ix, emu->iy);
		fprintf(file, "AF%d=%04"PRIX16",BC%d=%04"PRIX16",DE%d=%04"PRIX16",HL%d=%04"PRIX16"\n",
			1 - emu->af_bank, emu->bank[1 - emu->af_bank].af,
			1 - emu->main_bank, emu->bank[1 - emu->main_bank].bc,
			1 - emu->main_bank, emu->bank[1 - emu->main_bank].de,
			1 - emu->main_bank, emu->bank[1 - emu->main_bank].hl);
		fprintf(file, "PC=%04"PRIX16",I=%02"PRIX16",R=%02"PRIX16"\n",
			emu->pc, emu->i, emu->r);
		fprintf(file, "C=%d,N=%d,P/V=%d,H=%d,Z=%d,S=%d,C%d=%d,N%d=%d,P/V%d=%d,H%d=%d,Z%d=%d,S%d=%d,IFF1=%d,IFF2=%d,IM=%d,AF bank:%d,main bank:%d\n",
			(emu->bank[emu->af_bank].af&X86_FL_CF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_NF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_PF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_AF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_ZF)!=0,
			(emu->bank[emu->af_bank].af&X86_FL_SF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_CF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_NF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_PF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_AF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_ZF)!=0,
			1 - emu->af_bank, (emu->bank[1 - emu->af_bank].af&X86_FL_SF)!=0,
			emu->iff1, emu->iff2, emu->im, emu->af_bank, emu->main_bank);
		break;
	}
}

#include "parse.c"

x86_result_t x80_step(x80_state_t * emu, x86_state_t * emu86)
{
	if(emu->cpu_method != X80_CPUMETHOD_SEPARATE)
		return X86_RESULT(X86_RESULT_SUCCESS, 0); // TODO: better response when no processor present?

	emu->parser->index_prefix = NONE;
	return x80_parse(emu->parser, emu, emu86, emu86 != NULL && emu86->option_disassemble, true);
}

x86_result_t x86_step(x86_state_t * emu)
{
	emu->emulation_result = X86_RESULT(X86_RESULT_SUCCESS, 0);

	if(emu->halted)
		return X86_RESULT(X86_RESULT_HALT, 0);

	emu->current_exception = X86_EXC_CLASS_BENIGN;
	if(setjmp(emu->exc) != 0)
		return emu->emulation_result; /* exception occured */

	bool disassemble = emu->option_disassemble;
#define prs (emu->parser)
	DEBUG("[CPU]\t");
#undef prs

	if(x86_is_emulation_mode(emu))
	{
		emu->x80.parser->index_prefix = NONE;
		emu->emulation_result = x80_parse(emu->x80.parser, &emu->x80, emu, disassemble, true);
	}
	else
	{
		emu->old_xip = emu->xip;

		emu->parser->segment = NONE;
		emu->parser->source_segment = X86_R_DS;
		emu->parser->source_segment2 = X86_R_DS;
		emu->parser->source_segment3 = X86_R_DS;
		emu->parser->destination_segment = X86_R_ES;

		emu->parser->rep_prefix = X86_PREF_NOREP;
		emu->parser->simd_prefix = X86_PREF_NONE;
		emu->parser->lock_prefix = emu->parser->user_mode = false;

		emu->parser->address_size = emu->parser->code_size = x86_get_code_size(emu);
		emu->parser->operation_size = emu->parser->code_size == X86_SIZE_WORD ? X86_SIZE_WORD : X86_SIZE_DWORD;

		emu->parser->rex_prefix = emu->parser->rex_w = false;
		emu->parser->rex_r = emu->parser->rex_x = emu->parser->rex_b = 0;
		emu->parser->opcode_map = 0;
		emu->parser->vex_l = emu->parser->vex_v = emu->parser->evex_vb = emu->parser->evex_vx = emu->parser->evex_a = 0;
		emu->parser->evex_z = false;

		emu->parser->address_offset = 0;
		emu->parser->register_field = 0;
		x86_parse(emu->parser, emu, disassemble, true);
	}

	if((x86_memory_segmented_read8(emu, X86_R_TR, 0x64) & 0x01) != 0)
	{
		emu->dr[6] |= X86_DR6_BS;
		x86_trigger_interrupt(emu, X86_EXC_DB | X86_EXC_TRAP, 0);
		// TODO: what happens to the emulation result?
	}

	return emu->emulation_result;
}

void x80_disassemble(x80_parser_t * prs)
{
	uint16_t old_pc = prs->current_position;
	prs->index_prefix = NONE;
	x80_parse(prs, NULL, NULL, true, false);
	prs->current_position = old_pc; // TODO
}

void x86_disassemble(x86_parser_t * prs, x86_state_t * emu)
{
	if(emu != NULL && x86_is_emulation_mode(emu))
	{
		x80_disassemble(emu->x80.parser);
	}
	else
	{
		uoff_t old_xip = prs->current_position;

		prs->segment = NONE;
		prs->source_segment = X86_R_DS;
		prs->source_segment2 = X86_R_DS;
		prs->source_segment3 = X86_R_DS;
		prs->destination_segment = X86_R_ES;

		prs->rep_prefix = X86_PREF_NOREP;
		prs->simd_prefix = X86_PREF_NONE;
		prs->lock_prefix = prs->user_mode = false;

		prs->address_size = prs->code_size;
		prs->operation_size = prs->code_size == X86_SIZE_WORD ? X86_SIZE_WORD : X86_SIZE_DWORD;

		prs->rex_prefix = prs->rex_w = false;
		prs->rex_r = prs->rex_x = prs->rex_b = 0;
		prs->opcode_map = 0;
		prs->vex_l = prs->vex_v = prs->evex_vb = prs->evex_vx = prs->evex_a = 0;
		prs->evex_z = false;

		prs->address_offset = 0;
		prs->register_field = 0;
		x86_parse(prs, NULL, true, false);
		prs->current_position = old_xip; // TODO
	}
}

void x87_step(x86_state_t * emu)
{
	if(emu->x87.fpu_type == X87_FPU_NONE || emu->x87.fpu_type == X87_FPU_INTEGRATED)
		return;

	if((emu->x87.sw & X87_SW_B) == 0)
		return;
	if(setjmp(emu->exc) != 0)
		return; /* exception occured */

	bool disassemble = emu->option_disassemble;

#define prs (emu->parser)
	DEBUG("[FPU]\t");
#undef prs

	x87_parse(emu->parser, emu, false, emu->x87.next_fop, emu->x87.next_fcs, emu->x87.next_fip, emu->x87.next_fds, emu->x87.next_fdp, emu->x87.segment, emu->x87.offset,
		disassemble, true);
	if(emu->x87.fpu_type != X87_FPU_INTEGRATED)
	{
		switch(emu->x87.queued_operation)
		{
		case X87_OP_NONE:
			break;
		case X87_OP_FSAVE:
			emu->sr[X86_R_FDS] = emu->x87.queued_segment;
			x87_state_save_real_mode16(emu, X86_R_FDS, emu->x87.queued_offset);
			break;
		case X87_OP_FSTENV:
			emu->sr[X86_R_FDS] = emu->x87.queued_segment;
			x87_environment_save_real_mode16(emu, X86_R_FDS, emu->x87.queued_offset);
			break;
		}
	}
	emu->x87.sw &= ~X87_SW_B;
}

bool x80_hardware_interrupt(x80_state_t * emu, x80_interrupt_t exception_type, size_t data_length, void * data)
{
	switch(exception_type)
	{
	case X80_INT_INTR:
		if(emu->iff1 != 0)
		{
			switch(emu->im)
			{
			case 0:
				if(emu->peripheral_data_length != 0)
					return false; // pretend that nothing happened
				emu->peripheral_data_length = data_length;
				emu->peripheral_data = malloc(data_length);
				memcpy(emu->peripheral_data, data, data_length);
				emu->peripheral_data_pointer = 0;
				break;
			case 1:
				x80_push16(emu, emu->pc);
				emu->pc = 0x0038;
				break;
			case 2:
				if(data_length == 0)
					return false; // pretend that nothing happened
				x80_push16(emu, emu->pc);
				emu->pc = x80_memory_read16(emu, (emu->i << 8) | *(uint8_t *)data);
				break;
			}
			emu->iff1 = emu->iff2 = 0;
			return true;
		}
		break;
	case X80_INT_NMI:
		switch(emu->cpu_type)
		{
		case X80_CPU_I85:
			x80_push16(emu, emu->pc);
			emu->pc = 0x0024;
			emu->iff1 = 0;
			return true;
		case X80_CPU_Z80:
			x80_push16(emu, emu->pc);
			emu->pc = 0x0066;
			emu->iff1 = 0;
			return true;
		default:
			// 8080 does not understand NMIs
			break;
		}
		break;
	// 8085 specific
	case X80_INT_RST7_5:
		if(emu->cpu_type == X80_CPU_I85 && emu->iff1 != 0 && emu->m7_5 == 0)
		{
			x80_push16(emu, emu->pc);
			emu->pc = 0x003C;
			emu->iff1 = 0;
			return true;
		}
		break;
	case X80_INT_RST6_5:
		if(emu->cpu_type == X80_CPU_I85 && emu->iff1 != 0 && emu->m6_5 == 0)
		{
			x80_push16(emu, emu->pc);
			emu->pc = 0x0034;
			emu->iff1 = 0;
			return true;
		}
		break;
	case X80_INT_RST5_5:
		if(emu->cpu_type == X80_CPU_I85 && emu->iff1 != 0 && emu->m5_5 == 0)
		{
			x80_push16(emu, emu->pc);
			emu->pc = 0x002C;
			emu->iff1 = 0;
			return true;
		}
		break;
	}
	return false;
}

bool x86_hardware_interrupt(x86_state_t * emu, uint16_t exception_number, size_t data_length, void * data)
{
	if(emu->full_z80_emulation && x86_is_emulation_mode(emu))
	{
		return x80_hardware_interrupt(&emu->x80, exception_number == X86_EXC_NMI, data_length, data);
	}
	else
	{
		if((exception_number & X86_EXC_ICE) != 0)
		{
			switch(emu->cpu_type)
			{
			case X86_CPU_286:
				x86_ice_storeall_286(emu);
				return true;
			case X86_CPU_386:
			case X86_CPU_486:
				x86_ice_storeall_386(emu, 0x60000); // TODO: not sure
				return true;
			default:
				break;
			}
		}
		else if((exception_number & X86_EXC_SMI) != 0)
		{
			if(emu->cpu_traits.smm_format != X86_SMM_NONE)
			{
				x86_smi_attributes_t attributes;
				memset(&attributes, 0, sizeof attributes);
				attributes.source = X86_SMISRC_EXTERNAL;
				x86_smm_enter(emu, attributes);
				return true;
			}
		}
		else if(emu->_if != 0 || exception_number == X86_EXC_NMI)
		{
			//x86_trigger_interrupt(emu, exception_number, 0);
			if(setjmp(emu->exc) != 0)
				return true;
			x86_enter_interrupt(emu, exception_number, 0);
			return true;
		}
		return false;
	}
}

void x89_step(x86_state_t * emu)
{
	if(!emu->x89.present)
		return;

	bool disassemble = emu->option_disassemble;

	for(unsigned channel_number = 0; channel_number < 2; channel_number++)
	{
		if(emu->x89.channel[channel_number].running)
		{
#define prs (emu->parser)
			if((emu->x89.channel[channel_number].psw & X89_PSW_XF) == 0)
				DEBUG("[IOP%d]\t", channel_number);
			x89_channel_step(emu, channel_number, emu->option_disassemble, true);
#undef prs
		}
	}
}

void x89_disassemble(x89_parser_t * prs)
{
	x89_parse(prs, NULL, -1, true, false);
}


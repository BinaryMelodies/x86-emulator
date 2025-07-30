#ifndef __CPU_H
#define __CPU_H

#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include "support.h"

// generic mark for invalid register value
#define NONE (-1)

#define SIZE_8BIT  1
#define SIZE_16BIT 2
#define SIZE_32BIT 4
#define SIZE_64BIT 8

/*
	For feature handling, x86 CPUs are grouped into two main categories of CPUs: pre-CPUID and post-CPUID
	Pre-CPUID processors include the following features:
		* Intel 8086/8088
			Original CPU, 16-bit registers, real mode addressing only
			Linear memory space: 20 bits
		* Intel 80186/80188
			Several new instructions, undefined instruction error
		* NEC V20
			Includes 80186 instructions and several new ones
			8080 emulation
		* NEC µPD9002
			V20 compatible, Z80 emulation, including system instructions
		* NEC V33
			V20 compatible, no 8080 emulation
			Extended addressing mode supports 24-bit physical memory
		* NEC V25
			V20 compatible, no 8080 emulation, 8 register banks
		* NEC V55
			V25 cmopatible, 16 register banks
			New segment registers: DS2/DS3, shifted by 8 instead of 4 bits
			Linear memory space: 24 bits
		* Intel 286
			80186 compatible, protected mode
			Linear memory space: 24 bits
		* Intel 386
			80286 compatible, 32-bit registers, paging support
			Linear memory space: 32 bits
		* Intel 486
			80386 compatible
*/

enum x86_cpu_type_t
{
	X86_CPU_8086, // Intel 8086/8088 and compatibles
	X86_CPU_186, // Intel 80186/80188 and compatibles
	X86_CPU_V60, // NEC V60, only x86 emulation
	X86_CPU_V20, // NEC V20/V30/V40/V50
	X86_CPU_UPD9002, // NEC µPD9002
	X86_CPU_V33, // NEC V33/V53
	X86_CPU_V25, // NEC V25
	X86_CPU_V55, // NEC V55
	X86_CPU_286, // Intel 80286 and compatibles
	X86_CPU_386, // Intel 80386 and compatibles
	X86_CPU_486, // Intel 80486 and compatibles
	X86_CPU_INTEL, // Intel CPUs
	X86_CPU_586 = X86_CPU_INTEL,
	X86_CPU_AMD, // AMD CPUs
	X86_CPU_CYRIX, // Cyrix CPUs and derivatives
	X86_CPU_VIA, // Centaur/VIA/Zhaoxin CPUs
	X86_CPU_EXTENDED, // experimental emulator extensions
};
typedef enum x86_cpu_type_t x86_cpu_type_t;

/*
	Several CPUs have variants that differ only in minimal ways, to ease software maintenance, these are included as subtypes
*/
enum x86_cpu_subtype_t
{
	// V25 variants
	X86_CPU_V25_V25S = 1, // NEC V25 Software Guard

	// 80386 variants
	X86_CPU_386_386B0 = 1, // stepping B0 of Intel 80386 (IBTS and XBTS support)
	X86_CPU_386_376, // Intel 80376 (no real mode or 16-bit mode)
	X86_CPU_386_AM386, // AMD Am386 with system management mode

	// 80486 variants
	X86_CPU_486_486A = 1, // stepping A of Intel 80486 (CMPXCHG at different opcode)
	X86_CPU_486_AM486, // AMD Am486 with system management mode

	// Cyrix CPUs and derivatives
	X86_CPU_CYRIX_CX486SLC, // Cyrix Cx486SLC
	X86_CPU_CYRIX_CX486SLCE, // Cyrix Cx486SLC/e
	X86_CPU_CYRIX_5X86, // Cyrix 5x86
	X86_CPU_CYRIX_MEDIAGX, // Cyrix MediaGX
	X86_CPU_CYRIX_GXM, // Cyrix MediaGXm and NS Geode GXm
	X86_CPU_CYRIX_GX1, // NS Geode GX1
	X86_CPU_CYRIX_GX2, // NS Geode GX2 and AMD Geode GX
	X86_CPU_CYRIX_LX, // AMD Geode LX
	X86_CPU_CYRIX_6X86, // Cyrix 6x86
	X86_CPU_CYRIX_M2, // Cyrix M2
	X86_CPU_CYRIX_III, // VIA Cyrix III (Joshua core)
};
typedef enum x86_cpu_subtype_t x86_cpu_subtype_t;

/*
	Floating point coprocessors are also grouped under families, including 2 special values for no coprocessor or integrated coprocessor
*/
enum x87_fpu_type_t
{
	X87_FPU_NONE, // no floating point coprocessor
	X87_FPU_72291, // NEC µPD72291, incompatible with the 8087 (TODO: not supported)
	X87_FPU_8087, // Intel 8087
	X87_FPU_72091, // NEC µPD72091 (TODO: not supported)
	X87_FPU_287, // Intel 80287
	X87_FPU_387, // Intel 80387
	X87_FPU_EMC87, // Cyrix EMC87
	X87_FPU_IIT, // IIT 3C87
	X87_FPU_INTEGRATED,

	X87_FPU_187 = X87_FPU_387,
	X87_FPU_487 = X87_FPU_INTEGRATED,
};
typedef enum x87_fpu_type_t x87_fpu_type_t;

/*
	These variants provide a couple of additional instructions, not supported on later FPUs
*/
enum x87_fpu_subtype_t
{
	//
	X87_FPU_287_287XL = 1, // Intel 80287XL
	X87_FPU_387_387SL = 1,
};
typedef enum x87_fpu_subtype_t x87_fpu_subtype_t;

/* Convenient way to refer to operation sizes */
enum x86_operation_size_t
{
	X86_SIZE_BYTE = SIZE_8BIT,
	X86_SIZE_WORD = SIZE_16BIT,
	X86_SIZE_DWORD = SIZE_32BIT,
	X86_SIZE_LONG = X86_SIZE_DWORD,
	X86_SIZE_QWORD = SIZE_64BIT,
	X86_SIZE_QUAD = X86_SIZE_QWORD,
};
typedef enum x86_operation_size_t x86_operation_size_t;

/* before AMD64 */
//#define X86_GPR_COUNT 8 // general purpose registers: AX to DI
/* after AMD64 */
//#define X86_GPR_COUNT 16 // general purpose registers: RAX to RDI, R8 to R15
/* after APX */
#define X86_GPR_COUNT 32 // general purpose registers: RAX to RDI, R8 to R31

/* before 80386 */
//#define X86_SR_COUNT 4 // segment registers: ES to DS
/* after 80386 */
//#define X86_SR_COUNT 6 // segment registers: ES to DS, FS, GS
/* for V55, which includes segment registers 6, 7 */
#define X86_SR_COUNT 8 // segment registers: ES to DS, DS3, DS2, and FS, GS for 386+

/* before 80286 */
//#define X86_TABLEREG_COUNT 0
/* after 80286 */
#define X86_TABLEREG_COUNT 4 // GDTR, IDTR, LDTR, TR

/* before 80286 */
//#define X86_CR_COUNT 0 // MSW only
/* for 80286 */
//#define X86_CR_COUNT 1
/* since 80386 */
//#define X86_CR_COUNT 8
/* since AMD64 */
#define X86_CR_COUNT 16
/* after APX */
//#define X86_CR_COUNT 32

/* before 80386 */
//#define X86_DR_COUNT 0
/* since 80386 */
//#define X86_DR_COUNT 8
/* since AMD64 */
#define X86_DR_COUNT 16
/* after APX */
//#define X86_DR_COUNT 32

/* for 80386, 80486 and extensions */
//#define X86_TR386_COUNT 8
/* Pentium */
#define X86_TR386_COUNT 16
/* normally */
//#define X86_TR386_COUNT 0

/* before SSE */
//#define X86_SSE_COUNT 0
/* original SSE */
//#define X86_SSE_COUNT 8
/* since AMD64 */
//#define X86_SSE_COUNT 16
/* since AVX-512 */
#define X86_SSE_COUNT 32

/* original SSE */
//#define X86_SSE_BITS 128
/* since AVX */
//#define X86_SSE_BITS 256
/* since AVX-512 */
#define X86_SSE_BITS 512

/* before 80386 */
//#define UOFF_BITS 16
//typedef uint16_t uoff_t;
//typedef  int16_t soff_t;
//typedef uint32_t uaddr_t; // must be at least 20 bits (24 for V33/286)
//#define UADDR_MAX UINT32_MAX
/* before AMD64 */
//#define UOFF_BITS 32
//typedef uint32_t uoff_t;
//typedef  int32_t soff_t;
//typedef uint32_t uaddr_t;
//#define UADDR_MAX UINT32_MAX
/* since AMD64 */
#define UOFF_BITS 64
typedef uint64_t uoff_t;
typedef  int64_t soff_t;
typedef uint64_t uaddr_t;
#define UADDR_MAX UINT64_MAX

/* Convenient way to refer to x86 general purpose registers */
enum x86_regnum_t
{
	X86_R_AX = 0,
	X86_R_CX,
	X86_R_DX,
	X86_R_BX,
	X86_R_SP,
	X86_R_BP,
	X86_R_SI,
	X86_R_DI,
};
typedef enum x86_regnum_t x86_regnum_t;

/* Segment registers and table registers are very similar, so they are enumerated together */
enum x86_segnum_t
{
	X86_R_SEGMENT_NONE = -1, // value reserved for missing value

	/* Original 4 segment registers */
	X86_R_ES = 0,
	X86_R_CS,
	X86_R_SS,
	X86_R_DS,
#if X86_SR_COUNT > 4
	/* Since 80386 */
	X86_R_FS,
	X86_R_GS,
#endif
#if X86_SR_COUNT > 6
	/* V55 extended segment registers */
	X86_R_DS3,
	X86_R_DS2,
#endif

	// table registers and fake segment numbers

#if X86_TABLEREG_COUNT != 0
	/* Since 80286 */
	X86_R_GDTR = X86_SR_COUNT,
	X86_R_IDTR,
	X86_R_LDTR,
	X86_R_TR,
#endif
	/* Phantom register to use with 8087/80287/80387 emulation */
	X86_R_FDS = X86_SR_COUNT + X86_TABLEREG_COUNT,
	/* V55 segment prefix for internal RAM */
	X86_R_IRAM,
};
typedef enum x86_segnum_t x86_segnum_t;

/* Convenience names for 8-bit registers */
enum x86_byteregnum_t
{
	X86_R_AL = 0,
	X86_R_CL,
	X86_R_DL,
	X86_R_BL,
	X86_R_AH,
	X86_R_CH,
	X86_R_DH,
	X86_R_BH,
};
typedef enum x86_byteregnum_t x86_byteregnum_t;

/* Only used for IX/IY prefixes */
enum x80_regnum_t
{
	X80_R_IX = 1,
	X80_R_IY = 2,
};
typedef enum x80_regnum_t x80_regnum_t;


/* Intel 8089 general purpose registers */
enum x89_regnum_t
{
	/* 20-bit registers with tag */
	X89_R_GA = 0,
	X89_R_GB,
	X89_R_GC,
	/* 16-bit register */
	X89_R_BC,
	/* 20-bit register with tag, essentially a program counter */
	X89_R_TP,
	/* 16-bit registers */
	X89_R_IX,
	X89_R_CC,
	X89_R_MC,
};
typedef enum x89_regnum_t x89_regnum_t;

/* Repetition prefixes used for instructions */
enum x86_rep_prefix_t
{
	X86_PREF_NOREP,
	X86_PREF_REPNZ,
	X86_PREF_REPZ,
	/* NEC specific */
	X86_PREF_REPNC,
	X86_PREF_REPC,
};
typedef enum x86_rep_prefix_t x86_rep_prefix_t;

/* Instruction prefixes used for SIMD instructions */
enum x86_simd_prefix_t
{
	X86_PREF_NONE,
	X86_PREF_66,
	X86_PREF_F3,
	X86_PREF_F2,
};
typedef enum x86_simd_prefix_t x86_simd_prefix_t;

/* Exception numbers as well as flags passed as bits 8 and beyond for interrupt processing */
enum x86_exception_t
{
	X86_EXC_DE  = 0x00, /* divide by zero */
	X86_EXC_DB  = 0x01, /* debug */
	X86_EXC_NMI = 0x02, /* non-maskable interrupt */
	X86_EXC_BP  = 0x03, /* breakpoint */
	X86_EXC_OF  = 0x04, /* overflow */
	/* 80186+, V20 */
	X86_EXC_BR  = 0x05, /* bound range exceeded */
	/* 80186+ */
	X86_EXC_UD  = 0x06, /* undefined/invalid opcode */
	/* 80186+, V25 */
	X86_EXC_NM  = 0x07, /* no math/x87 device not available */
	/* V25 only */
	X86_EXC_IO  = 0x15, /* I/O instruction when ^IBRK is cleared */
	/* µPD9002 Z80 mode only */
	X86_EXC_IN  = 0x7C, /* executed on IN instructions */
	X86_EXC_OUT = 0x7D, /* executed on OUT instructions */
	X86_EXC_LDAR = 0x7E, /* executed on LD A, R */
	/* 80186 only */
	X86_EXC_TIMER0 = 0x08,
	X86_EXC_DMA0 = 0x0A,
	X86_EXC_DMA1 = 0x0B,
	X86_EXC_INT0 = 0x0C,
	X86_EXC_INT1 = 0x0D,
	X86_EXC_INT2 = 0x0E,
	X86_EXC_INT3 = 0x0F,
	X86_EXC_TIMER1 = 0x12,
	X86_EXC_TIMER2 = 0x13,
	/* 80286+ */
	X86_EXC_DF  = 0x08, /* double fault */
	/* 80286, 80386 only */
	X86_EXC_MP  = 0x09, /* math unit protection fault (also called coprocessor segment overrun) */
	/* 80286+ */
	X86_EXC_TS  = 0x0A, /* invalid TSS */
	X86_EXC_NP  = 0x0B, /* segment not present */
	X86_EXC_SS  = 0x0C, /* stack segment fault */
	X86_EXC_GP  = 0x0D, /* general protection fault */
	/* 80386+ */
	X86_EXC_PF  = 0x0E, /* page fault */
	X86_EXC_MF  = 0x10, /* math fault/coprocessor error */
	/* 80486+ */
	X86_EXC_AC  = 0x11, /* alignment check */
	/* later */
	X86_EXC_MC  = 0x12, /* machine check */
	X86_EXC_XM  = 0x13, /* SIMD floating point exception */ /* Intel name */
	X86_EXC_XF  = X86_EXC_XM, /* AMD name */
	X86_EXC_VE  = 0x14, /* virtualization exception */
	X86_EXC_CP  = 0x15, /* control protection exception */
	X86_EXC_HV  = 0x1C, /* hypervisor injection exception */
	X86_EXC_VC  = 0x1D, /* VMM communication exception */
	X86_EXC_SX  = 0x1E, /* security exception */
	/* internal flags used for interrupt management */
	X86_EXC_FAULT = 0x100,
	X86_EXC_TRAP = 0x200,
	X86_EXC_ABORT = 0x400,
	X86_EXC_VALUE = 0x800, /* interrupt pushes exception code on stack */
	X86_EXC_INT_N = 0x1000, /* originates from INT N instruction */
	X86_EXC_INT_SW = 0x2000, /* originates from INT3 or INTO instruction */
	X86_EXC_ICEBP = 0x4000, /* INT1 instruction (F1) */
	X86_EXC_ICE = 0x10000, /* in-circuit emulator interrupt */ // TODO
	X86_EXC_SMI = 0x20000, /* system management interrupt */ // TODO

	/* V60 specific (not part of x86) */
	V60_EXC_PI = 0x2000, /* privileged instruction */
	V60_EXC_RI = 0x2001, /* reserved instruction */
	V60_EXC_ZD = 0x2002, /* zero divide */
	V60_EXC_SST = 0x2003, /* single step trap */
	V60_EXC_O = 0x2004, /* overflow */
	V60_EXC_I = 0x2005, /* index */
	V60_EXC_CNP = 0x2006, /* coprocessor not present */
};

typedef enum x86_exception_t x86_exception_t;
/* External interrupt type, when issued to 8080/8085/Z80 */
enum x80_interrupt_t
{
	X80_INT_INTR = 0, // generic hardware interrupt
	X80_INT_NMI = X86_EXC_NMI, // Z80 and 8085 (TRAP) non-maskable interrupts
	// 8085 specific
	X80_INT_RST7_5,
	X80_INT_RST6_5,
	X80_INT_RST5_5,
};
typedef enum x80_interrupt_t x80_interrupt_t;

/* Descriptor table entry types in attributes word */
enum x86_descriptor_type_t
{
	X86_DESC_TYPE_LDT = 0x0200,
	X86_DESC_TYPE_TSS16_A = 0x0100,
	X86_DESC_TYPE_TSS16_B = 0x0300,
	X86_DESC_TYPE_CALLGATE16 = 0x0400,
	X86_DESC_TYPE_TASKGATE = 0x0500,
	X86_DESC_TYPE_INTGATE16 = 0x0600,
	X86_DESC_TYPE_TRAPGATE16 = 0x0700,

	X86_DESC_TYPE_TSS32_A = 0x0900,
	X86_DESC_TYPE_TSS32_B = 0x0B00,
	X86_DESC_TYPE_CALLGATE32 = 0x0C00,
	X86_DESC_TYPE_INTGATE32 = 0x0E00,
	X86_DESC_TYPE_TRAPGATE32 = 0x0F00,

	X86_DESC_TYPE_MASK = 0x0F00,
};
typedef enum x86_descriptor_type_t x86_descriptor_type_t;

/* Assorted register numbers and bits */
enum
{
	// MSR registers, accessed in emu->msr[]
	X86_R_MC_ADDR = 0x00000000,
	X86_R_MC_TYPE = 0x00000001,
	X86_R_TR1 = 0x00000002, // Pentium, WinChip
	X86_R_MSR_TEST_DATA = 0x00000003, // Cyrix 6x86MX+
	X86_R_TR2 = 0x00000004, // Pentium
	X86_R_MSR_TEST_ADDRESS = 0x00000004, // Cyrix 6x86MX+
	X86_R_TR3 = 0x00000005, // Pentium
	X86_R_MSR_COMMAND_STATUS = 0x00000005, // Cyrix 6x86MX+
	X86_R_TR4 = 0x00000006, // Pentium
	X86_R_TR5 = 0x00000007, // Pentium
	X86_R_TR6 = 0x00000008, // Pentium
	X86_R_TR7 = 0x00000009, // Pentium
	X86_R_TR8 = 0x0000000A, // Pentium
	X86_R_TR9 = 0x0000000B, // Pentium
	X86_R_TR10 = 0x0000000C, // Pentium
	X86_R_TR11 = 0x0000000D, // Pentium
	X86_R_TR12 = 0x0000000E, // Pentium, K6, WinChip
	X86_R_TSC = 0x00000010,
	X86_R_CESR = 0x00000011, // Pentium, Cyrix 6x86MX+, WinChip
	X86_R_CTR0 = 0x00000012, // Pentium, Cyrix 6x86MX+, WinChip
	X86_R_CTR1 = 0x00000013, // Pentium, Cyrix 6x86MX+, WinChip
	X86_R_APIC_BASE = 0x0000001B,
	X86_R_EBL_CR_POWERON = 0x0000002A, // Pentium Pro/II/III/4, VIA C3+
	X86_R_K5_AAR = 0x00000082, // K5
	X86_R_K5_HWCR = 0x00000083, // K5
	X86_R_SMBASE = 0x0000009E,
	X86_R_PERFCTR0 = 0x000000C1, // Pentium Pro/II/III, Geode GX+, VIA C3+
	X86_R_PERFCTR1 = 0x000000C2, // Pentium Pro/II/III, Geode GX+, VIA C3+
	X86_R_FCR_WINCHIP = 0x00000107, // WinChip, VIA moved it to 0x00001107
	X86_R_FCR1_WINCHIP = 0x00000108, // WinChip, VIA moved it to 0x00001108
	X86_R_FCR2_WINCHIP = 0x00000109, // WinChip, VIA moved it to 0x00001109
	X86_R_FCR3_WINCHIP = 0x0000010A, // WinChip C2+
	X86_R_BBL_CR_CTL3 = 0x0000011E, // Pentium II/III, VIA C3+
	X86_R_SYSENTER_CS = 0x00000174,
	X86_R_SYSENTER_ESP = 0x00000175,
	X86_R_SYSENTER_EIP = 0x00000176,
	X86_R_MCG_CAP = 0x00000179,
	X86_R_MCG_STATUS = 0x0000017A,
	X86_R_MCG_CTL = 0x0000017B,
	X86_R_PERFEVTSEL0 = 0x00000186,
	X86_R_PERFEVTSEL1 = 0x00000187,
	X86_R_DEBUGCTL = 0x000001D9,
	X86_R_LASTBRANCH_TOS = 0x000001DA, // Pentium 4
	X86_R_LASTBRANCHFROMIP = 0x000001DB, // Pentium Pro/II/III/4
	X86_R_LASTBRANCHTOIP = 0x000001DC, // Pentium Pro/II/III/4
	X86_R_LER_FROM_IP = 0x000001DD,
	X86_R_LER_TO_IP = 0x000001DE,
	X86_R_LER_INFO = 0x000001E0,
	X86_R_BNDCFGS = 0x00000D90,
	X86_R_FCR = 0x00001107, // VIA C3+
	X86_R_FCR1 = 0x00001108, // VIA C3+
	X86_R_FCR2 = 0x00001109, // VIA C3+
	X86_R_GX2_PCR = 0x00001250, // Geode GX2+
	X86_R_SMM_CTL = 0x00001301, // Geode GX2+
	X86_R_DMI_CTL = 0x00001302, // Geode GX2+
	X86_R_SMM_HDR = 0x0000132B, // Geode GX2+
	X86_R_DMM_HDR = 0x0000132C, // Geode GX2+
	X86_R_SMM_BASE = 0x0000133B, // Geode GX2+
	X86_R_DMM_BASE = 0x0000133C, // Geode GX2+
	X86_R_EFER = 0xC0000080,
	X86_R_STAR = 0xC0000081,
	X86_R_LSTAR = 0xC0000082,
	X86_R_CSTAR = 0xC0000083,
	X86_R_SF_MASK = 0xC0000084,
	X86_R_UWCCR = 0xC0000085, // K6-III
	X86_R_PSOR = 0xC0000087, // K6-III
	X86_R_PFIR = 0xC0000088, // K6-III
	X86_R_L2AAR = 0xC0000089, // K6-III
	X86_R_FS_BASE = 0xC0000100,
	X86_R_GS_BASE = 0xC0000101,
	X86_R_KERNEL_GS_BASE = 0xC0000102,
	X86_R_TSC_AUX = 0xC0000103,

	// FLAGS bits
	X86_FL_CF   = 0x0001,
	// for V25/V55
	X86_FL_IBRK_ = 0x0002,
	// for Z80
	X86_FL_NF   = 0x0002,
	X86_FL_PF   = 0x0004,
	// V25, flags copied over from the SFR FLAG
	X86_FLAG_MASK = 0x0028,
	X86_FL_AF   = 0x0010,
	X86_FL_ZF   = 0x0040,
	X86_FL_SF   = 0x0080,
	X86_FL_TF   = 0x0100,
	X86_FL_IF   = 0x0200,
	X86_FL_DF   = 0x0400,
	X86_FL_OF   = 0x0800,
	// for V25/V55
	X86_FL_RB_SHIFT = 12,
	X86_FL_V25_RB_MASK = 0x7000,
	X86_FL_V55_RB_MASK = 0xF000,
	// since 80286
	X86_FL_IOPL_MASK = 0x3000,
	X86_FL_IOPL_SHIFT = 12,
	// since 80286
	X86_FL_NT    = 0x4000,
	// for V20/µPD9002
	X86_FL_MD    = 0x8000,
	// since 80386
	X86_FL_RF    = 0x00010000,
	// since 80386
	X86_FL_VM    = 0x00020000,
	// since 80486
	X86_FL_AC    = 0x00040000,
	// since 80586
	X86_FL_VIF   = 0x00080000,
	// since 80586
	X86_FL_VIP   = 0x00100000,
	// since 80586
	X86_FL_ID    = 0x00200000,
	// VIA Padlock
	X86_FL_VIA_ACE = 0x40000000,
	// VIA C3 only
	X86_FL_AI    = 0x80000000,
	// V60 only (not actually part of the visible portion of the FLAGS register)
	X86_FL_CTL   = 0x80000000,

	// v33 only, extended addressing mode bits, accessed in emu->v33_xam
	X86_XAM_XA = 0x01,

	// v25 only, special function registers, accessed in emu->iram
	X86_SFR_FLAG = 0x1EA,
	X86_SFR_PRC  = 0x1EB,
	X86_PRC_RAMEN = 0x40,
	X86_V25_SFR_ISPR = 0x1FC,
	X86_SFR_IDB  = 0x1FF,

	// v55 only, accessed offset from 0xFFE00
	X86_V55_SFR_ISPR = 0x0C4,

	// x87 registers
	// Control word flags, accessed via emu->x87.cw
	X87_CW_IM = 0x0001, // invalid
	X87_CW_DM = 0x0002, // denormalized
	X87_CW_ZM = 0x0004, // zero divide
	X87_CW_OM = 0x0008, // overflow
	X87_CW_UM = 0x0010, // underflow
	X87_CW_PM = 0x0020, // precision
	X87_CW_IEM = 0x0080, // 1 for disabled (8087 only)
	X87_CW_PC_MASK = 0x0300,
	X87_CW_PC_SHIFT = 8,
	X87_CW_RC_MASK = 0x0C00,
	X87_CW_RC_SHIFT = 10,
	X87_CW_X  = 0x1000, // 8087, 287

	// Status word flags, accessed via emu->x87.sw
	X87_SW_IE = 0x0001,
	X87_SW_DE = 0x0002,
	X87_SW_ZE = 0x0004,
	X87_SW_OE = 0x0008,
	X87_SW_UE = 0x0010,
	X87_SW_PE = 0x0020,
	X87_SW_SF = 0x0040, // 387+
	X87_SW_IR = 0x0080, // 8087
	X87_SW_ES = 0x0080, // 287+
	X87_SW_C0 = 0x0100,
	X87_SW_C1 = 0x0200,
	X87_SW_C2 = 0x0400,
	X87_SW_TOP_MASK = 0x3800,
	X87_SW_TOP_SHIFT = 11,
	X87_SW_C3 = 0x4000,
	X87_SW_B  = 0x8000,

	// 387SL only
	X87_DW_S = 0x0100, // static bit

	// 286 or 386+
	// Flags, accessed via emu->cr[0]
	X86_CR0_PE = 0x00000001,
	X86_CR0_MP = 0x00000002,
	X86_CR0_EM = 0x00000004,
	X86_CR0_TS = 0x00000008,
	X86_CR0_ET = 0x00000010,
	X86_CR0_NE = 0x00000020,
	X86_CR0_WP = 0x00010000,
	X86_CR0_AM = 0x00040000,
	X86_CR0_NW = 0x20000000,
	X86_CR0_CD = 0x40000000,
	X86_CR0_PG = 0x80000000,

	// Flags, accessed via emu->cr[3]
	X86_CR3_PWT = 0x00000008,
	X86_CR3_PCD = 0x00000010,

	// Flags, accessed via emu->cr[4]
	X86_CR4_VME = 0x00000001,
	X86_CR4_PVI = 0x00000002,
	X86_CR4_TSD = 0x00000004,
	X86_CR4_DE  = 0x00000008,
	X86_CR4_PSE = 0x00000010,
	X86_CR4_PAE = 0x00000020,
	X86_CR4_MCE = 0x00000040,
	X86_CR4_PGE = 0x00000080,
	X86_CR4_PCE = 0x00000100,
	X86_CR4_OSFXSR = 0x00000200,
	X86_CR4_OSXMEX = 0x00000400,
	X86_CR4_UMIP = 0x00000800,
	X86_CR4_VA57 = 0x00001000,
	X86_CR4_VMXE = 0x00002000,
	X86_CR4_SMXE = 0x00004000,
	X86_CR4_SEE = 0x00008000,
	X86_CR4_FSGSBASE = 0x00010000,
	X86_CR4_PCIDE = 0x00020000,
	X86_CR4_OSXSAVE = 0x00040000,
	X86_CR4_KL  = 0x00080000,
	X86_CR4_SMEP = 0x00100000,
	X86_CR4_SMAP = 0x00200000,
	X86_CR4_PKE = 0x00400000,
	X86_CR4_CET = 0x00800000,
	X86_CR4_PKS = 0x01000000,
	X86_CR4_UNITR = 0x02000000,
	X86_CR4_LASS = 0x08000000,
	X86_CR4_FRED = 0x100000000,

	// Flags, accessed via emu->dr[6]
	X86_DR6_B0 = 0x00000001,
#define X86_DR6_B(__n) (X86_DR6_B0 << (__n))
	X86_DR6_B1 = 0x00000002,
	X86_DR6_B2 = 0x00000004,
	X86_DR6_B3 = 0x00000008,
	X86_DR6_BD = 0x00002000,
	X86_DR6_BS = 0x00004000,
	X86_DR6_BT = 0x00008000,
	/* TODO */
	X86_DR6_SMM = 0x00001000,
	X86_DR6_RTM = 0x00010000,

	// Flags, accessed via emu->dr[7]
	X86_DR7_L0 = 0x00000001,
#define X86_DR7_L(__n) (X86_DR7_L0 << ((__n) + 1))
	X86_DR7_G0 = 0x00000002,
#define X86_DR7_G(__n) (X86_DR7_G0 << ((__n) + 1))
	X86_DR7_L1 = 0x00000004,
	X86_DR7_G1 = 0x00000008,
	X86_DR7_L2 = 0x00000010,
	X86_DR7_G2 = 0x00000020,
	X86_DR7_L3 = 0x00000040,
	X86_DR7_G3 = 0x00000080,
	X86_DR7_LE = 0x00000100,
	X86_DR7_GE = 0x00000200,
	X86_DR7_RW0_MASK  = 0x00030000,
#define X86_DR7_RW_MASK(__n) (X86_DR7_RW0_MASK << ((__n) + 1))
	X86_DR7_RW0_SHIFT = 16,
#define X86_DR7_RW_SHIFT(__n) (X86_DR7_RW0_SHIFT + ((__n) * 2))
	X86_DR7_LEN0_MASK = 0x000C0000,
#define X86_DR7_LEN_MASK(__n) (X86_DR7_LEN0_MASK << ((__n) + 1))
	X86_DR7_LEN0_SHIFT = 18,
#define X86_DR7_LEN_SHIFT(__n) (X86_DR7_LEN0_SHIFT + ((__n) * 2))
	X86_DR7_RW1_MASK  = 0x00300000,
	X86_DR7_LEN1_MASK = 0x00C00000,
	X86_DR7_RW2_MASK  = 0x03000000,
	X86_DR7_LEN2_MASK = 0x0C000000,
	X86_DR7_RW3_MASK  = 0x30000000,
	X86_DR7_LEN3_MASK = 0xC0000000,

	X86_DR7_RTM = 0x00000800,
	X86_DR7_ICE = 0x00001000,
	X86_DR7_GD  = 0x00002000,
	X86_DR7_TB  = 0x00004000,
	X86_DR7_TT  = 0x00008000,

	/* 486 */
	// Flags, accessed via emu->tr386[4]
	X86_TR4_VALID_MASK = 0x00000078,
	X86_TR4_LRU_MASK   = 0x00000380,
	X86_TR4_V     = 0x00000400,

	/* 486 */
	// Flags, accessed via emu->tr386[5]
	X86_TR5_CTL_MASK   = 0x00000003,
	X86_TR5_ENT_MASK   = 0x0000000C,
	X86_TR5_SET_MASK   = 0x00007F00,

	/* 386, 486 */
	// Flags, accessed via emu->tr386[6]
	X86_TR6_C   = 0x00000001, /* command bit */
	X86_TR6_WX  = 0x00000020, /* -X (actually #) marks the complement */
	X86_TR6_W   = 0x00000040, /* R/W bit */
	X86_TR6_UX  = 0x00000080,
	X86_TR6_U   = 0x00000100, /* U/S bit */
	X86_TR6_DX  = 0x00000200,
	X86_TR6_D   = 0x00000400, /* dirty bit */
	X86_TR6_V   = 0x00000800, /* valid bit */

	/* 386, 487 */
	// Flags, accessed via emu->tr386[7]
	X86_TR7_REP_MASK = 0x0000000C, /* reported associated block */
	X86_TR7_HT  = 0x00000010, /* lookup hit (1) vs miss (0) */
	/* 486 */
	X86_TR7_PL    = X86_TR7_HT,
	X86_TR7_LRU_MASK   = 0x00000380,
	X86_TR7_PWT   = 0x00000400, /* page table entry PWT bit */
	X86_TR7_PCD   = 0x00000800, /* page table entry PCD bit */

	// Flags, accessed via emu->efer
	X86_EFER_SCE = 0x00000001,
	X86_EFER_LME = 0x00000100,
	X86_EFER_LMA = 0x00000400,
	X86_EFER_NXE = 0x00000800,
	X86_EFER_LMSLE = 0x00001000,
	X86_EFER_FFXSR = 0x00002000,
	X86_EFER_TCE = 0x00004000,
	X86_EFER_MCOM = 0x00020000,
	/* TODO: other fields */

	// Flags, accessed via emu->xcr0
	X86_XCR0_X87 = 0x00000001,
	X86_XCR0_XMM = 0x00000002,
	/* TODO: other fields */

	// Cyrix
	X86_CCR1_USE_SMI = 0x02,
	X86_CCR1_SMAC = 0x04,
	X86_CCR1_SM3 = 0x80,

	// Cyrix 5x86
	X86_CCR3_SMM_MODE = 0x04,
	X86_CCR3_MAPEN_MASK = 0xF0,
	X86_CCR3_MAPEN_SHIFT = 4,

	// Cyrix MediaGX
	X86_CCR3_SMI_LOCK = 0x01,
	X86_CCR3_MAPEN = 0x10,

	// Cyrix EMMI
	X86_CCR7_EMMX = 0x01,

	// Geode GX2/LX, smm_ctl
	X86_SMI_INST = 0x0008,
	// Geode GX2/LX, dmi_ctl
	X86_DMI_INST = 0x0001,
	// Geode GX2/LX, gx2_pcr
	X86_GX2_PCR_INV_3DNOW = 0x0002,

	// x87 tag word values, 3 bit bitfields in emu->x87.tw
	X87_TAG_VALID = 0,
	X87_TAG_ZERO = 1,
	X87_TAG_SPECIAL = 2,
	X87_TAG_EMPTY = 3,

	// REX prefix bits
	X86_REX_B = 0x01,
	X86_REX_X = 0x02,
	X86_REX_R = 0x04,
	X86_REX_W = 0x08,

	// Selector masks
	X86_SEL_RPL_MASK = 3,
	X86_SEL_LDT = 4,
	X86_SEL_INDEX_MASK = ~7,

	// Descriptor table entry byte offsets
	X86_DESCBYTE_ACCESS = 5,

	// Descriptor table entry word offsets
	X86_DESCWORD_TYPE = 2,
	X86_DESCWORD_ACCESS = 2,
	X86_DESCWORD_FLAGS = 3,

	X86_DESCWORD_SEGMENT_LIMIT0 = 0,
	X86_DESCWORD_SEGMENT_BASE0 = 1,
	X86_DESCWORD_SEGMENT_BASE1 = 2,
	X86_DESCWORD_SEGMENT_LIMIT1 = 3,
	X86_DESCWORD_SEGMENT_BASE2 = 3,

	X86_DESCWORD_GATE_OFFSET0 = 0,
	X86_DESCWORD_GATE_SELECTOR = 1,
	X86_DESCWORD_GATE_OFFSET1 = 3,
	X86_DESCWORD_GATE64_OFFSET2 = 4,
	X86_DESCWORD_GATE64_OFFSET3 = 5,

	// Descriptor table entry attributes, accessed at byte offset 4-7 in each entry
	X86_DESC_A = 0x0100,
	X86_DESC_R = 0x0200,
	X86_DESC_W = 0x0200,
	X86_DESC_BUSY = 0x0200, // TSS only
	X86_DESC_C = 0x0400,
	X86_DESC_E = 0x0400,
	X86_DESC_X = 0x0800,
	X86_DESC_S = 0x1000,
	X86_DESC_DPL_MASK = 0x6000,
	X86_DESC_DPL_SHIFT = 13,
	X86_DESC_P = 0x8000,
	X86_DESC_L = 0x00200000,
	X86_DESC_D = 0x00400000,
	X86_DESC_B = 0x00400000,
	X86_DESC_G = 0x00800000,

	// Page table entry bits
	X86_PAGE_ENTRY_P = 0x0001,
	X86_PAGE_ENTRY_WR = 0x0002,
	X86_PAGE_ENTRY_US = 0x0004,
	X86_PAGE_ENTRY_A = 0x0020,
	X86_PAGE_ENTRY_D = 0x0040,
	X86_PAGE_ENTRY_PS = 0x0080,
	X86_PAGE_ENTRY_XD = 0x8000000000000000,

	// CPUID features
	// CPUID EAX=0x00000001, result in EDX
	X86_CPUID1_EDX_FPU  = 0x00000001,
	X86_CPUID1_EDX_VME  = 0x00000002,
	X86_CPUID1_EDX_DE   = 0x00000004,
	X86_CPUID1_EDX_PSE  = 0x00000008,
	X86_CPUID1_EDX_TSC  = 0x00000010,
	X86_CPUID1_EDX_MSR  = 0x00000020,
	X86_CPUID1_EDX_PAE  = 0x00000040,
	X86_CPUID1_EDX_MCE  = 0x00000080,
	X86_CPUID1_EDX_CX8  = 0x00000100,
	X86_CPUID1_EDX_APIC = 0x00000200,
	X86_CPUID1_EDX_SEP  = 0x00000800,
	X86_CPUID1_EDX_MTRR = 0x00001000,
	X86_CPUID1_EDX_PGE  = 0x00002000,
	X86_CPUID1_EDX_MCA  = 0x00004000,
	X86_CPUID1_EDX_CMOV = 0x00008000,
	X86_CPUID1_EDX_PAT  = 0x00010000,
	X86_CPUID1_EDX_PSE36 = 0x00020000,
	X86_CPUID1_EDX_PSN  = 0x00040000,
	X86_CPUID1_EDX_CLFL = 0x00080000,
	X86_CPUID1_EDX_NX   = 0x00100000,
	X86_CPUID1_EDX_DTES = 0x00200000,
	X86_CPUID1_EDX_ACPI = 0x00400000,
	X86_CPUID1_EDX_MMX  = 0x00800000,
	X86_CPUID1_EDX_FXSR = 0x01000000,
	X86_CPUID1_EDX_SSE  = 0x02000000,
	X86_CPUID1_EDX_SSE2 = 0x04000000,
	X86_CPUID1_EDX_SS   = 0x08000000,
	X86_CPUID1_EDX_HTT  = 0x10000000,
	X86_CPUID1_EDX_TM1  = 0x20000000,
	X86_CPUID1_EDX_IA64 = 0x40000000,
	X86_CPUID1_EDX_PBE  = 0x80000000,

	// CPUID EAX=0x00000001, result in ECX
	X86_CPUID1_ECX_SSE3 = 0x00000001,
	X86_CPUID1_ECX_PCLMUL = 0x00000002,
	X86_CPUID1_ECX_DTES64 = 0x00000004,
	X86_CPUID1_ECX_MON  = 0x00000008,
	X86_CPUID1_ECX_DSCPL = 0x00000010,
	X86_CPUID1_ECX_VMX  = 0x00000020,
	X86_CPUID1_ECX_SMX  = 0x00000040,
	X86_CPUID1_ECX_EST  = 0x00000080,
	X86_CPUID1_ECX_TM2  = 0x00000100,
	X86_CPUID1_ECX_SSSE3 = 0x00000200,
	X86_CPUID1_ECX_CID  = 0x00000400,
	X86_CPUID1_ECX_SDBG = 0x00000800,
	X86_CPUID1_ECX_FMA  = 0x00001000,
	X86_CPUID1_ECX_CX16 = 0x00002000,
	X86_CPUID1_ECX_ETPRD = 0x00004000,
	X86_CPUID1_ECX_PDCM = 0x00008000,
	X86_CPUID1_ECX_PCID = 0x00020000,
	X86_CPUID1_ECX_DCA  = 0x00040000,
	X86_CPUID1_ECX_SSE41 = 0x00080000,
	X86_CPUID1_ECX_SSE42 = 0x00100000,
	X86_CPUID1_ECX_X2APIC = 0x00200000,
	X86_CPUID1_ECX_MOVBE = 0x00400000,
	X86_CPUID1_ECX_POPCNT = 0x00800000,
	X86_CPUID1_ECX_TSCD = 0x01000000,
	X86_CPUID1_ECX_AES = 0x02000000,
	X86_CPUID1_ECX_XSAVE = 0x04000000,
	X86_CPUID1_ECX_OSXSAVE = 0x08000000,
	X86_CPUID1_ECX_AVX  = 0x10000000,
	X86_CPUID1_ECX_F16C = 0x20000000,
	X86_CPUID1_ECX_RDRAND = 0x40000000,
	X86_CPUID1_ECX_HV   = 0x80000000,

	// CPUID EAX=0x80000001, result in EDX
	X86_CPUID_EXT1_EDX_SYSCALL_K6 = 0x00000400,
	X86_CPUID_EXT1_EDX_SYSCALL = 0x00000800,
	X86_CPUID_EXT1_EDX_FCMOV  = 0x00010000, // GXm
	X86_CPUID_EXT1_EDX_MP   = 0x00080000,
	X86_CPUID_EXT1_EDX_NX   = 0x00100000,
	X86_CPUID_EXT1_EDX_MMXPLUS = 0x00400000,
	X86_CPUID_EXT1_EDX_EMMI = 0x01000000, // GXm, Extended MMX
	X86_CPUID_EXT1_EDX_FFXSR = 0x02000000,
	X86_CPUID_EXT1_EDX_PG1G = 0x04000000,
	X86_CPUID_EXT1_EDX_TSCP = 0x08000000,
	X86_CPUID_EXT1_EDX_LM   = 0x20000000,
	X86_CPUID_EXT1_EDX_3DNOW = 0x40000000,
	X86_CPUID_EXT1_EDX_3DNOWEXT = 0x80000000,

	// CPUID EAX=0x80000001, result in ECX
	X86_CPUID_EXT1_ECX_AHF64 = 0x00000001,
	X86_CPUID_EXT1_ECX_CMP  = 0x00000002,
	X86_CPUID_EXT1_ECX_SVM  = 0x00000004,
	X86_CPUID_EXT1_ECX_EAS  = 0x00000008,
	X86_CPUID_EXT1_ECX_CR8D = 0x00000010,
	X86_CPUID_EXT1_ECX_LZCNT = 0x00000020,
	X86_CPUID_EXT1_ECX_SSE4A = 0x00000040,
	X86_CPUID_EXT1_ECX_MSSE = 0x00000080,
	X86_CPUID_EXT1_ECX_3DNOWP = 0x00000100,
	X86_CPUID_EXT1_ECX_OSVW = 0x00000200,
	X86_CPUID_EXT1_ECX_IBS  = 0x00000400,
	X86_CPUID_EXT1_ECX_XOP  = 0x00000800, // 0x8F prefix
	X86_CPUID_EXT1_ECX_SKINIT = 0x00001000,
	X86_CPUID_EXT1_ECX_WDT  = 0x00002000,
	X86_CPUID_EXT1_ECX_LWP  = 0x00008000,
	X86_CPUID_EXT1_ECX_FMA4 = 0x00010000,
	X86_CPUID_EXT1_ECX_TCE  = 0x00020000,
	X86_CPUID_EXT1_ECX_NODEIND = 0x00080000,
	X86_CPUID_EXT1_ECX_TBM  = 0x00200000,
	X86_CPUID_EXT1_ECX_TOPX = 0x00400000,
	X86_CPUID_EXT1_ECX_PCX_CORE = 0x00800000,
	X86_CPUID_EXT1_ECX_PCX_NB = 0x01000000,
	X86_CPUID_EXT1_ECX_DBX  = 0x04000000,
	X86_CPUID_EXT1_ECX_PERFTSC = 0x08000000,
	X86_CPUID_EXT1_ECX_PCX_L2I = 0x10000000,
	X86_CPUID_EXT1_ECX_MONX = 0x20000000,

	// CPUID EAX=0x8000001F, result in EAX
	X86_CPUID_EXT31_EAX_SEV_ES = 0x00000008,

	// CPUID EAX=0x00000007, ECX=0x00000000, result in EBX
	X86_CPUID7_0_EBX_FSGSBASE = 0x00000001,
	X86_CPUID7_0_EBX_BMI1 = 0x00000008,
	X86_CPUID7_0_EBX_SMEP = 0x00000080,
	X86_CPUID7_0_EBX_RDSEED = 0x00040000,
	X86_CPUID7_0_EBX_SMAP = 0x00010000,
	X86_CPUID7_0_EBX_AVX512_F = 0x00010000,
	X86_CPUID7_0_EBX_MPX = 0x00004000,
	// TODO: others
	// CPUID EAX=0x00000007, ECX=0x00000000, result in ECX
	X86_CPUID7_0_ECX_CET_SS = 0x00000080,
	// TODO: others

	// CPUID EAX=0x00000007, ECX=0x00000001, result in ECX
	X86_CPUID7_1_ECX_X86S = 0x00000004,
	// CPUID EAX=0x00000007, ECX=0x00000001, result in EDX
	X86_CPUID7_1_EDX_APX_F = 0x00200000,
	// TODO: others

	// Page fault exception value bits
	X86_EXC_VALUE_PF_P = 0x00000001,
	X86_EXC_VALUE_PF_WR = 0x00000002,
	X86_EXC_VALUE_PF_US = 0x00000004,
	X86_EXC_VALUE_PF_RSVD = 0x00000008,
	X86_EXC_VALUE_PF_ID = 0x00000010,

	// 80186 peripheral control block registers, accessed via emu->pcb[]
	X86_PCB_PCR = 0xFE >> 1,

	X86_PCB_PCR_ADDRESS = 0x0FFF,
	X86_PCB_PCR_MIO = 0x1000,
	X86_PCB_PCR_RMX = 0x4000, // TODO
	X86_PCB_PCR_ET = 0x8000, // TODO

	// V25/V55 register offsets in bank
	X86_BANK_DS2 = 0, // V55 specific segment register
	X86_BANK_VECTOR_PC, // vector IP
	X86_BANK_DS3 = X86_BANK_VECTOR_PC, // V55 specific segment register
	X86_BANK_PSW_SAVE, // FLAGS save
	X86_BANK_PC_SAVE, // IP save
	X86_BANK_DS0, // DS
	X86_BANK_SS,
	X86_BANK_PS, // CS
	X86_BANK_DS1, // ES
	X86_BANK_IY, // DI
	X86_BANK_IX, // SI
	X86_BANK_BP,
	X86_BANK_SP,
	X86_BANK_BW, // BX
	X86_BANK_DW, // DX
	X86_BANK_CW, // CX
	X86_BANK_AW, // AX

	// 8089
	// PSW bits, accessed via emu->x89.channel[channel_number].psw
	X89_PSW_D = 0x01,
	X89_PSW_S = 0x02,
	X89_PSW_TB = 0x04,
	X89_PSW_IC = 0x08,
	X89_PSW_IS = 0x10,
	X89_PSW_B = 0x20,
	X89_PSW_XF = 0x40,
	X89_PSW_P = 0x80,

	// CC bits, accessed via emu->x89.channel[channel_number].r[X89_R_CC]
	X89_CC_TSH_MASK = 0x0007,
	X89_CC_TSH_SHIFT = 0,
	X89_CC_TBC_MASK = 0x0018,
	X89_CC_TBC_SHIFT = 3,
	X89_CC_TX_MASK = 0x0060,
	X89_CC_TX_SHIFT = 5,
	X89_CC_TS = 0x0080,
	X89_CC_C = 0x0100,
	X89_CC_L = 0x0200,
	X89_CC_S = 0x0400,
	X89_CC_SYN_MASK = 0x1800,
	X89_CC_SYN_SHIFT = 11,
	X89_CC_TR = 0x2000,
	X89_CC_F0 = 0x4000,
	X89_CC_F1 = 0x8000,
};

/* Segment descriptor caches, also the layout for GDTR/LDTR */
struct x86_segment_t
{
	uint16_t selector;
	uaddr_t base;
	uint32_t limit;
	uint32_t access;
};
typedef struct x86_segment_t x86_segment_t;

/* SSE registers and their extensions (XMM/YMM/ZMM) */
union x86_sse_register_t
{
	uint8_t b[X86_SSE_BITS >> 3];
	uint16_t w[X86_SSE_BITS >> 4];
	uint32_t l[X86_SSE_BITS >> 5];
	uint64_t q[X86_SSE_BITS >> 6];
	float32_t s[X86_SSE_BITS >> 5];
	float64_t d[X86_SSE_BITS >> 6];
};
typedef union x86_sse_register_t x86_sse_register_t;

#if BYTE_ORDER == LITTLE_ENDIAN
# define XMM_B(i) b[(i)]
# define XMM_W(i) w[(i)]
# define XMM_L(i) l[(i)]
# define XMM_S(i) s[(i)]
#elif BYTE_ORDER == BIT_ENDIAN
# define XMM_B(i) b[(i) ^ 7]
# define XMM_W(i) w[(i) ^ 3]
# define XMM_L(i) l[(i) ^ 1]
# define XMM_S(i) s[(i) ^ 1]
#endif
#define XMM_Q(i) q[(i)]
#define XMM_D(i) d[(i)]

/* Current execution state of the CPU, typically running or halted */
enum x86_execution_state_t
{
	X86_STATE_RUNNING,
	X86_STATE_HALTED,
	X86_STATE_STOPPED, // v25/v55 specific
};
typedef enum x86_execution_state_t x86_execution_state_t;

/* Results from x86 execution, low 8 bits represent the type, remaining bits a value */
enum x86_result_t
{
	/* Execution succeeded */
	X86_RESULT_SUCCESS,
	/* Execution is in the middle of a (still running) string instruction */
	X86_RESULT_STRING,
	/* Execution halted (also includes stopped) */
	X86_RESULT_HALT,
	/* An interrupt was invoked, interrupt number is value, execution will continue */
	X86_RESULT_CPU_INTERRUPT,
	/* An ICE interrupt occured via software */
	X86_RESULT_ICE_INTERRUPT,
	/* The CPU is issuing an IRQ, IRQ number is value */
	X86_RESULT_IRQ,
	/* CPU triple faulted, execution halted */
	X86_RESULT_TRIPLE_FAULT,
	/* Interrupts should not be delivered until next instruction */
	X86_RESULT_INHIBIT_INTERRUPTS,
	/* An undefined instruction occured, however 186+ would report an X86_CPU_INTERRUPT instead */
	X86_RESULT_UNDEFINED,
};
typedef enum x86_result_t x86_result_t;

#define X86_RESULT_MASK ((x86_result_t)0xFF)
#define X86_RESULT_VALUE_SHIFT 8

#define X86_RESULT(__result, __value) (((__result) & X86_RESULT_MASK) | ((__value) << X86_RESULT_VALUE_SHIFT))

#define X86_RESULT_TYPE(__result) ((__result) & X86_RESULT_MASK)
#define X86_RESULT_VALUE(__result) ((__result) >> X86_RESULT_VALUE_SHIFT)

/* Class of exception, used to escalate to double/triple fault */
enum x86_exception_class_t
{
	X86_EXC_CLASS_BENIGN,
	X86_EXC_CLASS_CONTRIBUTORY,
	X86_EXC_CLASS_PAGE_FAULT,
	X86_EXC_CLASS_DOUBLE_FAULT,
};
typedef enum x86_exception_class_t x86_exception_class_t;

// Access types, these are encoded as such for Intel in DR7
enum x86_access_t
{
	X86_ACCESS_EXECUTE = 0,
	X86_ACCESS_WRITE = 1,
	X86_ACCESS_IO = 2,
	X86_ACCESS_READ = 3,
};
typedef enum x86_access_t x86_access_t;

/* CPU operating modes */
enum x86_cpu_level_t
{
	X86_LEVEL_USER, /* regular CPU operation */
	// since 386SL and Pentium
	X86_LEVEL_SMM, /* System management mode (also called ring -2) */
	// 286, 386, 486, also used for Am386/Am486 SMM
	X86_LEVEL_ICE, /* In-circuit emulation */
	// Geode GX2/GX
	X86_LEVEL_DMM, /* Debug management mode */ // TODO: not sure how this mode works
};
typedef enum x86_cpu_level_t x86_cpu_level_t;

/* The system management interrupt header format, how the registers are stored when entering SMM */
enum x86_smm_format_t
{
	X86_SMM_NONE,
	/* Intel */
	X86_SMM_80386SL,
	X86_SMM_P5,
	X86_SMM_P6,
	X86_SMM_P4,
	X86_SMM_INTEL64,
	/* AMD */
	X86_SMM_K5,
	X86_SMM_K6,
	X86_SMM_AMD64,
	/* Cyrix */
	X86_SMM_CX486SLCE,
	X86_SMM_M1,
	X86_SMM_M2,
	X86_SMM_MEDIAGX,
	X86_SMM_GX2, // TODO: check if GX2 worked like LX did
};
typedef enum x86_smm_format_t x86_smm_format_t;

/* Structure containing properties of the CPU, including what functionalities it supports */
typedef struct x86_cpu_traits_t x86_cpu_traits_t;
struct x86_cpu_traits_t
{
	x86_cpu_type_t cpu_type;
	x86_cpu_subtype_t cpu_subtype;

	const char * description;

	x87_fpu_type_t default_fpu;
	uint32_t supported_fpu_types;

	uint8_t prefetch_queue_size; // set to 0 if CPU can detect self modifying code

	x86_smm_format_t smm_format;

	// CPUID
	struct
	{
		uint32_t eax, ebx, edx, ecx;
	} cpuid0, cpuid1, cpuid7_0, cpuid7_1, cpuid_ext0, cpuid_ext1, cpuid_ext31;

	// other features
	bool amd_smm; // repurposes ICEBP/LOADALL as SMI/RES3
	bool cpuid; // CPUID available
	bool rdpmc; // RDPMC available
	bool multibyte_nop; // NOP Eb available
	bool sse_non_simd; // SSE, non-SIMD instructions
	bool sse_simd; // SSE, SIMD instructions
	bool l10m; // Larrabee and KNF specific
	bool mvex; // KNC specific
	bool drex; // AMD DREX support (never implemented)
	// Cyrix features
	bool rdshr; // RDSHR instructions
	bool media_gx; // MediaGX instructions
	bool emmi; // EMMI support (also CPUID flag)
	bool dmm; // debug management mode supported
	bool _3dnow_gx; // PFRCPV available
	// VIA features
	bool altinst; // alternate instruction set supported
};

/* Describes the origin of the system management interrupt */
enum x86_smi_source_t
{
	X86_SMISRC_EXTERNAL,
	X86_SMISRC_SMINT, // Cyrix only
	X86_SMISRC_IO,
	X86_SMISRC_MEMORY, // MediaGX
};
typedef enum x86_smi_source_t x86_smi_source_t;

/* Type of I/O instruction causing the SMI, these are the encodings used by Intel */
enum x86_io_type_t
{
	X86_OUT_DX = 0,
	X86_IN_DX = 1,
	X86_OUTS = 2,
	X86_INS = 3,
	X86_REP_OUTS = 6,
	X86_REP_INS = 7,
	X86_OUT_IMM = 8,
	X86_IN_IMM = 9,
};
typedef enum x86_io_type_t x86_io_type_t;

/* Information needed to pass on when writing to the SMM save state */
struct x86_smi_attributes_t
{
	x86_smi_source_t source;
	x86_operation_size_t write_size; // only for X86_SMISRC_IO
	x86_io_type_t io_type; // only for X86_SMISRC_IO
	uint32_t write_address;
	uint32_t write_data; // only for X86_SMISRC_IO
	bool nested_smi; // 6x86MX and MediaGX
	bool vga_access; // MediaGX
};
typedef struct x86_smi_attributes_t x86_smi_attributes_t;

/* SMM revision identifier flags, used by Intel/AMD */
enum x86_revid_flags_t
{
	SMM_REVID_IO_RESTART = 0x00010000,
	SMM_REVID_SMBASE_RELOC = 0x00020000,
};
typedef enum x86_revid_flags_t x86_revid_flags_t;

union x86_mmx_t
{
	uint8_t b[8];
	uint16_t w[4];
	uint32_t l[2];
	uint64_t q[1];
	float32_t s[2];
	float64_t d[1];
};
typedef union x86_mmx_t x86_mmx_t;

#if BYTE_ORDER == LITTLE_ENDIAN
# define MMX_B(i) b[(i)]
# define MMX_W(i) w[(i)]
# define MMX_L(i) l[(i)]
# define MMX_S(i) s[(i)]
#elif BYTE_ORDER == BIT_ENDIAN
# define MMX_B(i) b[(i) ^ 7]
# define MMX_W(i) w[(i) ^ 3]
# define MMX_L(i) l[(i) ^ 1]
# define MMX_S(i) s[(i) ^ 1]
#endif
#define MMX_Q(i) q[(i)]
#define MMX_D(i) d[(i)]

/* Represents an x87/MMX register */
struct x87_register_t
{
#if _SUPPORT_FLOAT80
	/* Values are either stored as a native float, or as a sequence of fields */
	bool isfp;
#endif
	union
	{
		float80_t f;
		struct
		{
			x86_mmx_t mmx;
			uint16_t exponent;
		};
	};
};
typedef struct x87_register_t x87_register_t;

struct x86_tile_register_t
{
	// TODO
	struct
	{
		uint8_t byte[64];
	} row[16];
};

typedef struct x86_tile_register_t x86_tile_register_t;

/* Intel 8089 represents addresses as 20 bits with a tag to switch between memory and I/O space (or local space) */
typedef struct x89_address_t x89_address_t;
struct x89_address_t
{
	uint32_t address : 20;
	uint32_t tag : 1;
};

/* Intel 8089 context */
typedef struct x89_parser_t x89_parser_t;
struct x89_parser_t
{
	uint32_t current_position;

	char debug_output[64];

	uint8_t (* fetch8)(x89_parser_t *);
	uint16_t (* fetch16)(x89_parser_t *);
};

#if BYTE_ORDER == LITTLE_ENDIAN
# define _DEFINE_X89_WORD_REGISTER(__name) \
union \
{ \
	x89_address_t __name##_address; \
	uint16_t __name; \
}
#elif BYTE_ORDER == BIG_ENDIAN
# define _DEFINE_X89_WORD_REGISTER(__name) \
union \
{ \
	x89_address_t __name##_address; \
	struct \
	{ \
		uint32_t __name##_high : 4; \
		uint16_t __name; \
	} \
} __attribute__((packed))
#endif

/* Intel 8089 state for a single channel */
typedef struct x89_channel_state_t x89_channel_state_t;
struct x89_channel_state_t
{
	union
	{
		x89_address_t r[8];
		struct
		{
			x89_address_t ga, gb, gc;
			_DEFINE_X89_WORD_REGISTER(bc);
			x89_address_t tp;
			_DEFINE_X89_WORD_REGISTER(ix);
			_DEFINE_X89_WORD_REGISTER(cc);
			_DEFINE_X89_WORD_REGISTER(mcc);
		};
	};
	uint32_t pp : 20;
	uint8_t psw;
	bool running;
	bool start_transfer; // for 1 instruction delay
};

/* Full Intel 8089 state */
typedef struct x89_state_t x89_state_t;
struct x89_state_t
{
	bool present;
	bool initialized;
	uint16_t sysbus;
	uint16_t soc;
	x89_channel_state_t channel[2];
	uint32_t cp : 20;
};

enum x80_cpu_type_t
{
	X80_CPU_I80,
	X80_CPU_V20 = X80_CPU_I80, // treated identically to 8080
	X80_CPU_I85,
	X80_CPU_Z80,
	X80_CPU_UPD9002 = X80_CPU_Z80, // treated identically to Z80
};
typedef enum x80_cpu_type_t x80_cpu_type_t;

/* Whether an 8080 CPU is included or emulated */
enum x80_cpu_method_t
{
	X80_CPUMETHOD_NONE, // no CPU present
	X80_CPUMETHOD_SEPARATE, // separate CPU
	X80_CPUMETHOD_EMULATED, // emulated, as for V20/µPD9002
};
typedef enum x80_cpu_method_t x80_cpu_method_t;

/* Parsing state for 8080/8085/Z80 */
typedef struct x80_parser_t x80_parser_t;
struct x80_parser_t
{
	uint16_t current_position; // the current location
	x80_cpu_type_t cpu_type;
	x80_cpu_method_t cpu_method; // whether the CPU is separate from the x86 or emulated in chip
	bool use_intel8080_syntax; // default debug syntax is Zilog format, Intel format is offered as alternative

	x80_regnum_t index_prefix; // DD: and FD: prefixes */

	char debug_output[256];

	uint8_t (* fetch8)(x80_parser_t *);
	uint16_t (* fetch16)(x80_parser_t *);
};

typedef struct x80_state_t x80_state_t;
struct x80_state_t
{
	// by placing these fields inside a union, the same pointer can be used as an x80_parser_t or x80_state_t instance
	union
	{
		x80_parser_t parser[1]; // stored as array to be handled as a pointer
		struct
		{
			uint16_t pc; // the program counter (shares position with parser->current_position)
			x80_cpu_type_t cpu_type;
			x80_cpu_method_t cpu_method;
		};
	};

	// general purpose registers
	struct
	{
		uint16_t af, bc, de, hl;
	} bank[2];
	uint16_t sp, ix, iy;

	// Z80 control registers (IFF1 is also used on the 8080/8085)
	uint8_t r, i;
	unsigned iff1 : 1, iff2 : 1;
	unsigned im : 2;
	unsigned af_bank : 1, main_bank : 1;

	// 8085 specific
	unsigned m5_5 : 1, m6_5 : 1, m7_5 : 1;

	// emulates 8080 interrupts
	size_t peripheral_data_length;
	uint8_t * peripheral_data;
	size_t peripheral_data_pointer;

	void (* memory_fetch)(x80_state_t * emu, uint16_t address, void * buffer, size_t count);
	void (* memory_read)(x80_state_t * emu, uint16_t address, void * buffer, size_t count);
	void (* memory_write)(x80_state_t * emu, uint16_t address, const void * buffer, size_t count);
	uint8_t (* port_read)(x80_state_t * emu, uint16_t port);
	void (* port_write)(x80_state_t * emu, uint16_t port, uint8_t value);
};

/* The x87 floating point coprocessor state */
typedef struct x87_state_t x87_state_t;
struct x87_state_t
{
	/* Floating point unit */
	x87_fpu_type_t fpu_type;
	x87_fpu_subtype_t fpu_subtype;

	/* IIT 3c87 organizes registers into 4 banks, but not the status or tag words */
	struct
	{
		/* Registers with their absolute indexes instead of TOP relative indexes */
		x87_register_t fpr[8];
	} bank[4];
	/* IIT 3c87 current bank number */
	unsigned current_bank;

	/* Control, status and tag registers */
	uint16_t cw, sw, tw;

	/*
		Exception registers, used only by error handlers

		Floating point OP (opcode), CS (code segment), DS (data segment), IP (instruction pointer), DP (data pointer) registers
		FOP stores all 16 bits (since NEC reserves 66/67 as well as the D8-DF prefixes)
		FCS/FDS are only used in protected mode

		Note that IP/DP are 20-bit on an 8087 and they store the linear address
	*/
	uint16_t fop, fcs, fds;
	uaddr_t fip, fdp;

	/* Whether a 80287 is in protected mode, later FPUs directly check the CPU state */
	bool protected_mode;

	/* 387SL exclusive constant registers */
	uint16_t dw; // device word register
	uint16_t sg; // signature word register

	/* The following fields are used to let the x86 and x87 operate asynchronously */
	/* Operand data read by the main CPU, for memory operands */
	uint8_t operand_data[2];

	/* Stored for asynchronous execution, required to pass the 8087/80287/80387 parameters */
	uint16_t next_fop, next_fcs, next_fds;
	uaddr_t next_fip, next_fdp;

	/* Memory operand segment number and offset, required for asynchronouns access by 8087/80287/80387 */
	/* Note that for the 8087/80287/80387, this will reference the phantom register X86_R_FDS */
	x86_segnum_t segment;
	uint16_t offset;

	/* A parameter returned when the x87 issues an external interrupt */
	uint8_t irq_number;

	/* The 8087 is capable of queuing an operation after it finishes the currently running one */
	enum
	{
		X87_OP_NONE,
		X87_OP_FSAVE,
		X87_OP_FSTENV,
	} queued_operation;
	x86_segment_t queued_segment; // this will be copied back to X86_R_FDS
	uint16_t queued_offset;

	/* Replica of the address_text field, kept here for the debugger for asynchronous behavior */
	char address_text[24];
};

/* Entire state of the x86 instruction parser, including current prefixes and the memory/register operand */
typedef struct x86_parser_t x86_parser_t;
struct x86_parser_t
{
	uoff_t current_position;
	union
	{
		// easier access to cpu type
		x86_cpu_type_t cpu_type;
		x86_cpu_traits_t cpu_traits;
	};
	/* NEC CPUs use a different assembly syntax */
	bool use_nec_syntax;

	// a copy of the FPU type and subtype
	x87_fpu_type_t fpu_type;
	x87_fpu_subtype_t fpu_subtype;

	/* V25 software guard */
	// a 256 byte array that translates the first byte of each instruction in Secure Mode (MD=0)
	const uint8_t (* opcode_translation_table)[256];

	/* Current state of the instruction parser, note that there is some redundancy to the information stored here to simplify coding */

	/* Code size (16/32/64) */
	x86_operation_size_t code_size;
	/* Current operation size, can change due to prefixes */
	x86_operation_size_t operation_size;
	/* Current operation size, can change due to prefixes */
	x86_operation_size_t address_size;
	/* Segment provided as segment prefix, with more specific fields for specific registers */
	x86_segnum_t segment;
	/* Source segment for string instructions (never DS3 or IRAM) */
	x86_segnum_t source_segment;
	x86_segnum_t source_segment2; /* may only be DS2 or DS0 (DS) */
	x86_segnum_t source_segment3; /* may be DS1 (ES), IRAM but not DS3 */
	/* Destination segment for string instructions (only ES/DS1, DS3 or IRAM), needed because V55 allows multiple segment prefixes on the same instruction */
	x86_segnum_t destination_segment;
	/* REPZ/REPNZ (or REPC/REPNC) prefixes */
	x86_rep_prefix_t rep_prefix;
	/* SIMD prefixes (0x66/0xF3/0xF2), information also stored in operation_size and rep_prefix */
	x86_simd_prefix_t simd_prefix;

	/* The ModRM byte and its corresponding fields */
	uint8_t modrm_byte;
	int register_field; // value of the register field (bits 3 to 5)
	uoff_t address_offset; // calculated offset (only for memory operands)
	bool ip_relative; // needed to add EIP/RIP after the instruction has been fetched
	char address_text[24]; // textual representation of the (memory) operand

	/* Whether a LOCK prefix (0xF0) is present */
	bool lock_prefix;
	/* Set when the next instruction should access user mode memory, relevant for in-circuit emulation (286, 386), included here because it works as a prefix on the 286 */
	bool user_mode;

	// REX prefix
	bool rex_prefix; // set when REX prefix is present, required as it influences 8-bit register access
	bool rex_w; // set when REX.W is set, information also stored in operation_size
	int rex_r;
	int rex_x;
	int rex_b;
	// VEX/XOP prefix
	uint8_t opcode_map; // selects an opcode map
	int vex_l;
	int vex_v;
	// EVEX/MVEX prefix
	int evex_vb; // vector B (as register operand, not base register)
	int evex_vx; // vector X (as index operand)
	bool evex_z;
	int evex_a;

	char debug_output[256];

	uint8_t (* fetch8)(x86_parser_t *);
	uint16_t (* fetch16)(x86_parser_t *);
	uint32_t (* fetch32)(x86_parser_t *);
	uint64_t (* fetch64)(x86_parser_t *);
};

#if BYTE_ORDER == LITTLE_ENDIAN
# if UOFF_BITS >= 64
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint64_t __r64; \
	uint32_t __r32; \
	uint16_t __r16; \
	struct \
	{ \
		uint8_t __r8l, __r8h; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint64_t __r64; \
	uint32_t __r32; \
	uint16_t __r16; \
	uint8_t __r8; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint64_t __r64; \
	uint32_t __r32; \
	uint16_t __r16; \
}
# elif UOFF_BITS >= 32
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint32_t __r32; \
	uint16_t __r16; \
	struct \
	{ \
		uint8_t __r8l, __r8h; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint32_t __r32; \
	uint16_t __r16; \
	uint8_t __r8; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint32_t __r32; \
	uint16_t __r16; \
}
# else
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint16_t __r16; \
	struct \
	{ \
		uint8_t __r8l, __r8h; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint16_t __r16; \
	uint8_t __r8; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint16_t __r16; \
}
# endif
#elif BYTE_ORDER == BIG_ENDIAN
# if UOFF_BITS >= 64
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint64_t __r64; \
	struct \
	{ \
		uint32_t _##__r32, __r32; \
	}; \
	struct \
	{ \
		uint16_t _##__r16[3], __r16; \
	}; \
	struct \
	{ \
		uint8_t _##__r8l[6], __r8h, __r8l; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint64_t __r64; \
	struct \
	{ \
		uint32_t _##__r32, __r32; \
	}; \
	struct \
	{ \
		uint16_t _##__r16[3], __r16; \
	}; \
	struct \
	{ \
		uint8_t _##__r8[7], __r8; \
	}; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint64_t __r64; \
	struct \
	{ \
		uint32_t _##__r32, __r32; \
	}; \
	struct \
	{ \
		uint16_t _##__r16[3], __r16; \
	}; \
}
# elif UOFF_BITS >= 32
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint32_t __r32; \
	struct \
	{ \
		uint16_t _##__r16, __r16; \
	}; \
	struct \
	{ \
		uint8_t _##__r8l[2], __r8h, __r8l; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint32_t __r32; \
	struct \
	{ \
		uint16_t _##__r16, __r16; \
	}; \
	struct \
	{ \
		uint8_t _##__r8, __r8; \
	}; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint32_t __r32; \
	struct \
	{ \
		uint16_t _##__r16, __r16; \
	}; \
}
# else
#  define _DEFINE_HIGH_LOW_REGISTER(__r64, __r32, __r16, __r8h, __r8l) \
union \
{ \
	uint16_t __r16; \
	struct \
	{ \
		uint8_t __r8h, __r8l; \
	}; \
}
#  define _DEFINE_SIMPLE_REGISTER(__r64, __r32, __r16, __r8) \
union \
{ \
	uint16_t __r16; \
	struct \
	{ \
		uint8_t _r##__r8, __r8; \
	}; \
}
#  define _DEFINE_POINTER_REGISTER(__r64, __r32, __r16) \
union \
{ \
	uint16_t __r16; \
}
# endif
#endif

#define _DEFINE_SEGMENT_REGISTER(__name) \
union \
{ \
	uint16_t __name; \
	x86_segment_t __name##_cache; \
}
#define _DEFINE_TABLE_REGISTER(__name) \
struct \
{ \
	struct \
	{ \
		uint16_t __s; \
		uaddr_t base; \
		uint32_t limit; \
	} __name; \
}

/* The complete x86 emulation state */
typedef struct x86_state_t x86_state_t;
struct x86_state_t
{
	// by placing these fields inside a union, the same pointer can be used as an x80_parser_t or x80_state_t instance
	union
	{
		x86_parser_t parser[1];
		struct
		{
			/* The instruction pointer IP/EIP/RIP */
			union
			{
				uoff_t xip;
				_DEFINE_POINTER_REGISTER(rip, eip, ip); // convenient identifiers
			};

			/* CPU traits */
			union
			{
				// easier access to cpu type
				x86_cpu_type_t cpu_type;
				x86_cpu_traits_t cpu_traits;
			};
		};
	};

	/* General purpose registers, such as RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8-R31, see X86_R_* constants */
	union
	{
		uoff_t gpr[X86_GPR_COUNT];
		_DEFINE_SIMPLE_REGISTER(q, d, w, b) r[X86_GPR_COUNT]; // convenient identifiers
		struct
		{
			_DEFINE_HIGH_LOW_REGISTER(rax, eax,  ax, ah, al); // convenient identifiers
			_DEFINE_HIGH_LOW_REGISTER(rcx, ecx,  cx, ch, cl); // convenient identifiers
			_DEFINE_HIGH_LOW_REGISTER(rdx, edx,  dx, dh, dl); // convenient identifiers
			_DEFINE_HIGH_LOW_REGISTER(rbx, ebx,  bx, bh, bl); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (rsp, esp,  sp,     spl); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (rbp, ebp,  bp,     bpl); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (rsi, esi,  si,     sil); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (rdi, edi,  di,     dil); // convenient identifiers
#if X86_GPR_COUNT > 8
			_DEFINE_SIMPLE_REGISTER  (r8,  r8d,  r8w,    r8b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r9,  r9d,  r9w,    r9b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r10, r10d, r10w,   r10b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r11, r11d, r11w,   r11b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r12, r12d, r12w,   r12b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r13, r13d, r13w,   r13b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r14, r14d, r14w,   r14b); // convenient identifiers
			_DEFINE_SIMPLE_REGISTER  (r15, r15d, r15w,   r15b); // convenient identifiers
#endif
#if X86_GPR_COUNT > 16
			_DEFINE_SIMPLE_REGISTER  (r16, r16d, r16w,   r16b);
			_DEFINE_SIMPLE_REGISTER  (r17, r17d, r17w,   r17b);
			_DEFINE_SIMPLE_REGISTER  (r18, r18d, r18w,   r18b);
			_DEFINE_SIMPLE_REGISTER  (r19, r19d, r19w,   r19b);
			_DEFINE_SIMPLE_REGISTER  (r20, r20d, r20w,   r20b);
			_DEFINE_SIMPLE_REGISTER  (r21, r21d, r21w,   r21b);
			_DEFINE_SIMPLE_REGISTER  (r22, r22d, r22w,   r22b);
			_DEFINE_SIMPLE_REGISTER  (r23, r23d, r23w,   r23b);
			_DEFINE_SIMPLE_REGISTER  (r24, r24d, r24w,   r24b);
			_DEFINE_SIMPLE_REGISTER  (r25, r25d, r25w,   r25b);
			_DEFINE_SIMPLE_REGISTER  (r26, r26d, r26w,   r26b);
			_DEFINE_SIMPLE_REGISTER  (r27, r27d, r27w,   r27b);
			_DEFINE_SIMPLE_REGISTER  (r28, r28d, r28w,   r28b);
			_DEFINE_SIMPLE_REGISTER  (r29, r29d, r29w,   r29b);
			_DEFINE_SIMPLE_REGISTER  (r30, r30d, r30w,   r30b);
			_DEFINE_SIMPLE_REGISTER  (r31, r31d, r31w,   r31b);
#endif
		};
	};

	/*
		Segment registers (ES, CS, SS, DS, FS, GS and DS3, DS2 for V55)
		Also including table registers and the task state segment register since they have a similar format (GDTR, IDTR, LDTR, TR)
		Others (DS for the 8087/80287/80387 specifically)
	*/
	union
	{
		x86_segment_t sr[X86_SR_COUNT + X86_TABLEREG_COUNT + 1];
		struct
		{
			_DEFINE_SEGMENT_REGISTER(es);
			_DEFINE_SEGMENT_REGISTER(cs);
			_DEFINE_SEGMENT_REGISTER(ss);
			_DEFINE_SEGMENT_REGISTER(ds);
#if X86_SR_COUNT > 4
			_DEFINE_SEGMENT_REGISTER(fs); // 386+
			_DEFINE_SEGMENT_REGISTER(gs); // 386+
#endif
#if X86_SR_COUNT > 6
			_DEFINE_SEGMENT_REGISTER(ds3); // v55 only
			_DEFINE_SEGMENT_REGISTER(ds2); // v55 only
#endif
#if X86_TABLEREG_COUNT > 0
			_DEFINE_TABLE_REGISTER(gdtr); // 286+
			_DEFINE_TABLE_REGISTER(idtr); // 286+
			_DEFINE_SEGMENT_REGISTER(ldtr); // 286+
			_DEFINE_SEGMENT_REGISTER(tr); // 286+
#endif
		};
	};

	/* 8086 only internal registers */
	uint16_t ind_register, opr_register, tmpb_register;

	/* Current execution state, typically running or halted */
	x86_execution_state_t state;
	/* Current privilege level, stored separately because different CPU types store it in different places */
	int cpl;

	union
	{
		/* V25/V55 register banks, always stored here separately in little endian format, see X86_BANK_* constants */
		// Note that unlike an actual V25/V55, registers are also stored in the gpr/sr arrays and they must be synchronized with the bank values
		union
		{
			uint16_t w[16];
			struct
			{
				uint8_t ds2[2]; // V55
				union
				{
					uint8_t ds3[2]; // V55
					uint8_t vector_pc[2];
				};
				uint8_t psw_save[2];
				uint8_t pc_save[2];
				uint8_t ds0[2];
				uint8_t ss[2];
				uint8_t ps[2];
				uint8_t ds1[2];
				uint8_t ix[2];
				uint8_t iy[2];
				uint8_t bp[2];
				uint8_t sp[2];
				uint8_t bw[2];
				uint8_t dw[2];
				uint8_t cw[2];
				uint8_t aw[2];
			};
		} bank[16];
		/* V25 internal data area, first 256 bytes mapped to the register banks, the rest are SFRs; the V55 chip RAM uses standard memory access */
		uint8_t iram[512];
	};

	// 186
	/* Peripheral control block, stored in little endian format */
	uint16_t pcb[128];

	/* 8080 emulation (for NEC V20) and Z80 emulation (for µPD9002), the state must be synchronized with the main state */
	x80_state_t x80;

	/* Flags are stored separately, since different CPUs store these values in different places */
	// 8086+ carry flag, value is 0 or X86_FL_CF
	unsigned cf;
	// 8086+ parity flag, value is 0 or X86_FL_PF
	unsigned pf;
	// 8086+ auxiliary flag, value is 0 or X86_FL_AF
	unsigned af;
	// 8086+ zero flag, value is 0 or X86_FL_ZF
	unsigned zf;
	// 8086+ sign flag, value is 0 or X86_FL_SF
	unsigned sf;
	// 8086+ trap flag, value is 0 or X86_FL_TF
	unsigned tf;
	// 8086+ interrupt enabled flag, value is 0 or X86_FL_IF
	unsigned _if;
	// 8086+ direction flag, value is 0 or X86_FL_DF
	unsigned df;
	// 8086+ overflow flag, value is 0 or X86_FL_OF
	unsigned of;
	// 80286+ I/O privilege level, value is 0 to 3 (unshifted)
	unsigned iopl;
	// 80286+ I/O nexted task, value is 0 or X86_FL_NT
	unsigned nt;
	// V25, V55 I/O break flag, value is 0 or X86_FL_CF
	unsigned ibrk_;
	// V25, V55 register bank, value is 0 to 7 on a V25 (unshifted) or 0 to 15 on a V55 (unshifted)
	unsigned rb;
	// V20, µPD9002 mode flag, value is 0 (8080 emulation) or X86_FL_MD (native 8086 execution)
	unsigned md;
	// V20, µPD9002 mode flag write enable, set to true if the mode flag can be modified, not an actual flag
	bool md_enabled;
	// µPD9002 miscellaneous flags, value is a subset of the bits 0x2A
	unsigned z80_flags;
	// µPD9002 full Z80 emulation, not an actual flag
	bool full_z80_emulation;
	// 80386+ resume flag, value is 0 or X86_FL_RF
	uint32_t rf;
	// 80386+ virtual 8086 mode flag, value is 0 or X86_FL_VM
	uint32_t vm;
	// 80486+ alignment check flag, value is 0 or X86_FL_AC
	uint32_t ac;
	// 80586+ virtual interrupt flag, value is 0 or X86_FL_VIF
	uint32_t vif;
	// 80586+ virtual interrupt pending, value is 0 or X86_FL_VIP
	uint32_t vip;
	// 80586+ CPUID flag, value is 0 or X86_FL_ID
	uint32_t id;
	// VIA Padlock, AES key schedule loaded flag, value is 0 or X86_FL_VIA_ACE
	uint32_t via_ace;
	// VIA C3 "Nehemiah" only (C5XL), alternate instruction set, value is 0 or X86_FL_AI
	uint32_t ai;
	// v60 I/O control flag, value is 0 or X86_FL_CTL, not part of the x86 mode FLAGS register, but included for completeness
	uint32_t v60_ctl;

	union
	{
		struct
		{
			/* V33 page dictionary, stored in little endian format */
			uint16_t v33_pgr[64];
			// value is X86_XAM_XA if paging is enabled
			uint8_t v33_xam;
		};
		// V33 internal registers, accessed as a byte array
		uint8_t v33_io[0x81];
	};

	/* x87 floating point coprocessor/unit context */
	x87_state_t x87;

	// 386+
	/* Control registers, the lower 16 bits of CR0 can also be accessed via the MSW register */
	union
	{
		uoff_t cr[X86_CR_COUNT];
#if X86_CR_COUNT > 0
# if BYTE_ORDER == LITTLE_ENDIAN
		uint16_t msw;
# elif BYTE_ORDER == BIG_ENDIAN
#  if UOFF_BITS >= 64
		struct
		{
			uint16_t __crw[3], msw;
		};
#  elif UOFF_BITS >= 32
		struct
		{
			uint16_t __crw, msw;
		};
#  else
		uint16_t msw;
#  endif
# endif
#endif
	};
	/* Debug registers */
	uoff_t dr[X86_DR_COUNT];
	// 386, 486 and p5
	/* Test registers, accessed as MSRs on p5 */
	uoff_t tr386[X86_TR386_COUNT];

	// p5
	/* Pentium machine check address */
	uint64_t mc_addr;
	/* Pentium machine check type */
	uint64_t mc_type;
	/* Control and even select register */
	uint64_t msr_cesr;
	/* Counter 0 */
	uint64_t msr_ctr0;
	/* Counter 1 */
	uint64_t msr_ctr1;
	/* Time stamp counter MSRs */
	uint64_t tsc;

	// p6
	uint64_t apic_base;
	/* Processor hard power-on configuration */
	uint64_t msr_ebl_cr_poweron;
	/* Performance counter register 0 */
	uint64_t msr_perfctr0;
	/* Performance counter register 1 */
	uint64_t msr_perfctr1;
	/* Global machine check capability */
	uint64_t mcg_cap;
	/* Global machine check status */
	uint64_t mcg_status;
	/* Global machine check control */
	uint64_t mcg_ctl;
	/* Performance event select register 0 */
	uint64_t perfevtsel0;
	/* Performance event select register 1 */
	uint64_t perfevtsel1;
	/* Trace/profile resource control */
	uint64_t debugctl;
	uint64_t msr_lastbranch_tos; // p4
	uint64_t msr_lastbranchfromip;
	uint64_t msr_lastbranchtoip;
	uint64_t ler_from_ip;
	uint64_t ler_to_ip;
	uint64_t ler_info;

	// p6-2
	/* Control register 3 */
	uint64_t msr_bbl_cr_ctl3;
	/* SYSENTER/SYSEXIT MSRs */
	uint64_t sysenter_cs, sysenter_esp, sysenter_eip;

	// k5
	/* Array access register */
	uint64_t msr_k5_aar;
	/* k5 Hardware configuration register */
	uint64_t msr_k5_hwcr;

	// k6-2, x86-64
	/* Extended feature enable register MSRs */
	uint64_t efer;
	/* SYSCALL/SYSRET MSRs */
	uint64_t star, lstar, cstar, sf_mask;
	// k6-3
	/* UC/WC cacheability control register */
	uint64_t msr_uwccr;
	/* Processor state observability register */
	uint64_t msr_psor;
	/* Page flush/invalidate register */
	uint64_t msr_pfir;
	/* Level-2 cache array */
	uint64_t msr_l2aar;

	// Cyrix 6x86MX
	uint64_t cyrix_test_data;
	uint64_t cyrix_test_address;
	uint64_t cyrix_command_status;

	// winchip
	/* Feature control registers */
	uint64_t msr_fcr, msr_fcr1, msr_fcr2, msr_fcr3;

	// x86-64
	/* Kernel GS base MSR */
	uint64_t kernel_gs_base;
	/* Auxiliary TSC */
	uint64_t tsc_aux;

	// sse
	/* XMM/YMM/ZMM registers */
	x86_sse_register_t xmm[X86_SSE_COUNT];
	/* Control/status register */
	uint32_t mxcsr;

	// xsave (Penryn/Bulldozer additions)
	/* Extended control register 0 */
	uint64_t xcr0;

	// avx-512
	union
	{
		uint8_t b[8];
		uint16_t w[4];
		uint32_t l[2];
		uint64_t q[1];
	} k[8];

	// mpx (Skylake)
	struct
	{
		uint64_t lower_bound;
		uint64_t upper_bound;
	} bnd[4];

	uint64_t bndcfgs, bndcfgu, bndstatus;

	/* AMX */
	x86_tile_register_t tmm[8];
	uint64_t tilecfg;

	/* Cyrix configuration control registers */
	bool port22_accessed; // whether a configuration control register is available at the next I/O instruction on port 0x23
	uint8_t port_number;

	uint8_t ccr[8];
	uint32_t arr[14];
	// 5x86 or later
	uint8_t pcr[2];
	uint8_t dir[5];
	// MediaGX or later
	uint32_t smm_hdr;
	uint8_t gcr;
	uint8_t vgactl; // TODO: bit 0: SMI from A0000, bit 1: SMI from B0000, bit 2: SMI from B8000
	uint32_t vgam;
	// 6x86 or later
	uint8_t rcr[14];
	// Cyrix III
	uint8_t lcr1;
	uint8_t bcr[2];

	/* AMD Geode GX2+ specific registers */
	uint32_t dmm_hdr;
	struct
	{
		uint32_t base, limit;
	} smm, dmm;
	uint32_t smm_ctl; // TODO: bit 0 is SMM_NMI, 2 is SMM_NEST, 3 is SMI_INST, 4 is SMI_IO for I/O generated SMI, bit 5 is SMI_EXTL
	uint32_t dmi_ctl; // TODO: bit 0 is DMI_INST, bit 3 is DMI_DBG, bit 4 is DMI_ICEBP, TODO: other bits
	// pipeline control register
	uint32_t gx2_pcr;

	/* ICE and SMM */
	x86_cpu_level_t cpu_level; // the current state of the CPU (usually X86_LEVEL_USER)
	// Intel, AMD Geode GX2+
	uint32_t smbase;
	// Intel, AMD
	uint32_t smm_revision_identifier;

	// last I/O instruction information on SMI entry
	x86_io_type_t io_type;
	uoff_t io_restart_xdi;
	uoff_t io_restart_xsi;
	uoff_t io_restart_xcx;

	/* Intel 8089 state */
	x89_state_t x89;

	void (* memory_read)(x86_state_t * emu, x86_cpu_level_t level, uaddr_t address, void * buffer, size_t count);
	void (* memory_write)(x86_state_t * emu, x86_cpu_level_t level, uaddr_t address, const void * buffer, size_t count);
	void (* port_read)(x86_state_t * emu, uint16_t port, void * buffer, size_t count);
	void (* port_write)(x86_state_t * emu, uint16_t port, const void * buffer, size_t count);

	// CPU execution state
	x86_result_t emulation_result; // result to return from emulation function
	jmp_buf exc[2]; // target to jump to on exceptions
	enum
	{
		FETCH_MODE_NORMAL = 0,
		FETCH_MODE_PREFETCH = 1, // also avoid generating interrupts for prefetch
	} fetch_mode; // used to index the exc[] variable
	uoff_t old_xip; // IP/EIP/RIP on instruction start, used for faults
	x86_exception_class_t current_exception; // the class of the latest exception (benign if none occured), required to escalate to double/triple fault

	// queue of data bytes read during execution
#define X86_PREFETCH_QUEUE_MAX_SIZE 16
	uint8_t prefetch_queue[X86_PREFETCH_QUEUE_MAX_SIZE];
	uint8_t prefetch_queue_data_size; // number of actual bytes in queue
	uint8_t prefetch_queue_data_offset; // offset to first data byte in queue
	uoff_t prefetch_pointer; // offset (within CS) of next byte to fetch

	// structure to restart a REP or WAIT instruction
	struct
	{
		// must be 0 or one of 6C-6F, 9B, A4-A7, AA-AF
		uint8_t opcode;
		uoff_t xip_afterwards;
		x86_operation_size_t operation_size;
		x86_operation_size_t address_size;
		x86_segnum_t source_segment;
		x86_segnum_t source_segment3;
		x86_segnum_t destination_segment;
		x86_rep_prefix_t rep_prefix;
	} restarted_instruction;

	bool option_disassemble; // set to true to fill parser->debug_output with a disassembled instruction
};

//// Emulator interface

static inline uoff_t min(uoff_t a, uoff_t b)
{
	return a <= b ? a : b;
}

static inline uoff_t max(uoff_t a, uoff_t b)
{
	return a >= b ? a : b;
}

/* FLAGS is not present as a field in x86_state_t, these functions provide a convenient way to set or get the value */
uint64_t x86_flags_get64(x86_state_t * emu);
void x86_flags_set64(x86_state_t * emu, uint64_t value);

// convenience functions

static inline bool x86_is_nec(void * arg)
{
	x86_parser_t * prs = arg;
	return prs->cpu_type >= X86_CPU_V60 && prs->cpu_type <= X86_CPU_V55;
}

static inline bool x86_is_z80(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_UPD9002;
}

static inline bool x86_is_32bit_only(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_386 && emu->cpu_traits.cpu_subtype == X86_CPU_386_376;
}

static inline bool x86_is_ia64(x86_state_t * emu)
{
	return (emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_IA64) != 0;
}

static inline bool x86_traits_is_long_mode_supported(x86_cpu_traits_t * traits)
{
	return (traits->cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
}

static inline bool x86_is_emulation_mode_supported(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002 || emu->cpu_type == X86_CPU_EXTENDED;
}

static inline bool x86_is_long_mode_supported(x86_state_t * emu)
{
	return (emu->cpu_traits.cpuid_ext1.edx & X86_CPUID_EXT1_EDX_LM) != 0;
}

// This is an extension to the x86 architecture
static inline bool x86_is_long_vm86_supported(x86_state_t * emu)
{
	return emu->cpu_type == X86_CPU_EXTENDED;
}

static inline bool x86_is_real_mode(x86_state_t * emu)
{
	return (emu->cr[0] & X86_CR0_PE) == 0;
}

static inline unsigned x86_native_state_flag(x86_state_t * emu)
{
	return emu->cpu_type != X86_CPU_EXTENDED ? X86_FL_MD : 0;
}

static inline unsigned x86_emulation_state_flag(x86_state_t * emu)
{
	return emu->cpu_type != X86_CPU_EXTENDED ? 0 : X86_FL_MD;
}

static inline bool x86_is_emulation_mode(x86_state_t * emu)
{
	return x86_is_emulation_mode_supported(emu) && emu->md == x86_emulation_state_flag(emu);
}

static inline bool x86_is_long_mode(x86_state_t * emu)
{
	return !x86_is_real_mode(emu) && (emu->efer & X86_EFER_LMA) != 0;
}

static inline bool x86_is_protected_mode(x86_state_t * emu)
{
	return !x86_is_real_mode(emu) && (emu->efer & X86_EFER_LMA) == 0;
}

static inline bool x86_is_virtual_8086_mode(x86_state_t * emu)
{
	return (x86_is_protected_mode(emu) || (x86_is_long_mode(emu) && x86_is_long_vm86_supported(emu))) && emu->vm != 0;
}

static inline bool x86_is_64bit_mode(x86_state_t * emu)
{
	return x86_is_long_mode(emu) && (emu->sr[X86_R_CS].access & X86_DESC_L) != 0;
}

static inline bool x86_is_32bit_mode(x86_state_t * emu)
{
	return !x86_is_64bit_mode(emu) && (emu->sr[X86_R_CS].access & X86_DESC_D) != 0;
}

static inline bool x86_is_16bit_mode(x86_state_t * emu)
{
	return !x86_is_64bit_mode(emu) && (emu->sr[X86_R_CS].access & X86_DESC_D) == 0;
}

static inline x86_operation_size_t x86_get_code_size(x86_state_t * emu)
{
	if(x86_is_64bit_mode(emu))
	{
		return SIZE_64BIT;
	}
	else if(x86_is_32bit_mode(emu))
	{
		return SIZE_32BIT;
	}
	else if(!x86_is_emulation_mode(emu))
	{
		return SIZE_16BIT;
	}
	else
	{
		return SIZE_8BIT;
	}
}

// External reads/writes are relevant if there are separate internal address spaces (only relevant for V25)
void x86_memory_read_external(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer);
void x86_memory_write_external(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer);

void x86_memory_read(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer);
void x86_memory_write(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer);

static inline uint8_t x86_memory_read8_external(x86_state_t * emu, uaddr_t address)
{
	uint8_t value;
	x86_memory_read_external(emu, address, 1, &value);
	return value;
}

static inline uint16_t x86_memory_read16_external(x86_state_t * emu, uaddr_t address)
{
	uint16_t value;
	x86_memory_read_external(emu, address, 2, &value);
	return le16toh(value);
}

static inline uint32_t x86_memory_read32_external(x86_state_t * emu, uaddr_t address)
{
	uint32_t value;
	x86_memory_read_external(emu, address, 4, &value);
	return le32toh(value);
}

static inline uint64_t x86_memory_read64_external(x86_state_t * emu, uaddr_t address)
{
	uint64_t value;
	x86_memory_read_external(emu, address, 8, &value);
	return le64toh(value);
}

static inline void x86_memory_write8_external(x86_state_t * emu, uaddr_t address, uint8_t value)
{
	x86_memory_write_external(emu, address, 1, &value);
}

static inline void x86_memory_write16_external(x86_state_t * emu, uaddr_t address, uint16_t value)
{
	value = htole16(value);
	x86_memory_write_external(emu, address, 2, &value);
}

static inline uint8_t x86_memory_read8(x86_state_t * emu, uaddr_t address)
{
	uint8_t result;
	x86_memory_read(emu, address, 1, &result);
	return result;
}

static inline uint16_t x86_memory_read16(x86_state_t * emu, uaddr_t address)
{
	uint16_t result;
	x86_memory_read(emu, address, 2, &result);
	return le16toh(result);
}

static inline uint32_t x86_memory_read32(x86_state_t * emu, uaddr_t address)
{
	uint32_t result;
	x86_memory_read(emu, address, 4, &result);
	return le32toh(result);
}

static inline uint64_t x86_memory_read64(x86_state_t * emu, uaddr_t address)
{
	uint64_t result;
	x86_memory_read(emu, address, 8, &result);
	return le64toh(result);
}

static inline void x86_memory_write8(x86_state_t * emu, uaddr_t address, uint8_t value)
{
	x86_memory_write(emu, address, 1, &value);
}

static inline void x86_memory_write16(x86_state_t * emu, uaddr_t address, uint16_t value)
{
	value = htole16(value);
	x86_memory_write(emu, address, 2, &value);
}

static inline void x86_memory_write32(x86_state_t * emu, uaddr_t address, uint32_t value)
{
	value = htole32(value);
	x86_memory_write(emu, address, 4, &value);
}

static inline void x86_memory_write64(x86_state_t * emu, uaddr_t address, uint64_t value)
{
	value = htole64(value);
	x86_memory_write(emu, address, 8, &value);
}

void x80_reset(x80_state_t * emu, bool reset);
// Note: also resets the x87
void x86_reset(x86_state_t * emu, bool reset);

bool x80_hardware_interrupt(x80_state_t * emu, x80_interrupt_t exception_type, size_t data_length, void * data);

/*
 * When in 8086 mode (which is the case outside NEC µPD9002), exception_number is an 8-bit value and data/data_length are ignored
 * when in full 8080/Z80 emulation mode, exception_number can be any value from the x80_interrupt_t enumeration,
 * and data must contain the instruction bytes (for interrupt mode 0) such as an RST or CALL, or vector offset (for interrupt mode 3)
 */
bool x86_hardware_interrupt(x86_state_t * emu, uint16_t exception_number, size_t data_length, void * data);

void x80_debug(FILE * file, x80_state_t * emu);
void x86_debug(FILE * file, x86_state_t * emu);

void x80_disassemble(x80_parser_t * prs);
// Note: the emu argument can be NULL, but optionally it can be used to determine if the CPU is in 8080 emulation mode
void x86_disassemble(x86_parser_t * prs, x86_state_t * emu);

x89_parser_t * x89_setup_parser(x86_state_t * emu, unsigned channel_number);
void x89_disassemble(x89_parser_t * prs);

// Note: the emu86 argument is optional
x86_result_t x80_step(x80_state_t * emu, x86_state_t * emu86);
x86_result_t x86_step(x86_state_t * emu);
// Note: this only needs calling if the FPU is not integrated
void x87_step(x86_state_t * emu);
void x89_step(x86_state_t * emu);

// various CPUID Vendor IDs
#define X86_CPUID_VENDOR_INTEL         .ebx = 0x756E6547, .edx = 0x49656E69, .ecx = 0x6C65746E // "GenuineIntel"
#define X86_CPUID_VENDOR_INTEL_ALT     .ebx = 0x756E6547, .edx = 0x49656E69, .ecx = 0x6C65746F // "GenuineIotel"
#define X86_CPUID_VENDOR_AMD           .ebx = 0x68747541, .edx = 0x69746E65, .ecx = 0x444D4163 // "AuthenticAMD"
#define X86_CPUID_VENDOR_AMD_OLD       .ebx = 0x69444D41, .edx = 0x74656273, .ecx = 0x21726574 // "AMDisbetter!"
#define X86_CPUID_VENDOR_AMD_OLD2      .ebx = 0x20444D41, .edx = 0x45425349, .ecx = 0x52455454 // "AMD ISBETTER"
#define X86_CPUID_VENDOR_CYRIX         .ebx = 0x69727943, .edx = 0x736E4978, .ecx = 0x64616574 // "CyrixInstead"
#define X86_CPUID_VENDOR_CENTAUR       .ebx = 0x746E6543, .edx = 0x48727561, .ecx = 0x736C7561 // "CentaurHauls"
#define X86_CPUID_VENDOR_TRANSMETA_OLD .ebx = 0x6E617254, .edx = 0x74656D73, .ecx = 0x55504361 // "TransmetaCPU"
#define X86_CPUID_VENDOR_TRANSMETA     .ebx = 0x756E6547, .edx = 0x54656E69, .ecx = 0x3638784D // "GenuineTMx86"
#define X86_CPUID_VENDOR_NSC           .ebx = 0x646F6547, .edx = 0x79622065, .ecx = 0x43534E20 // "Geode by NSC"
#define X86_CPUID_VENDOR_NEXGEN        .ebx = 0x4778654E, .edx = 0x72446E65, .ecx = 0x6E657669 // "NexGenDriven"
#define X86_CPUID_VENDOR_RISE          .ebx = 0x65736952, .edx = 0x65736952, .ecx = 0x65736952 // "RiseRiseRise"
#define X86_CPUID_VENDOR_SIS           .ebx = 0x20536953, .edx = 0x20536953, .ecx = 0x20536953 // "SiS SiS SiS "
#define X86_CPUID_VENDOR_UMC           .ebx = 0x20434D55, .edx = 0x20434D55, .ecx = 0x20434D55 // "UMC UMC UMC "
#define X86_CPUID_VENDOR_VORTEX86      .ebx = 0x74726F56, .edx = 0x36387865, .ecx = 0x436F5320 // "Vortex86 SoC"
#define X86_CPUID_VENDOR_ZHAOXIN       .ebx = 0x68532020, .edx = 0x68676E61, .ecx = 0x20206961 // "  Shanghai  "
#define X86_CPUID_VENDOR_HYGON         .ebx = 0x6F677948, .edx = 0x6E65476E, .ecx = 0x656E6975 // "HygonGenuine"
#define X86_CPUID_VENDOR_RDC           .ebx = 0x756E6547, .edx = 0x20656E69, .ecx = 0x43445220 // "Genuine  RDC"
#define X86_CPUID_VENDOR_ELBRUS        .ebx = 0x204B3245, .edx = 0x4843414D, .ecx = 0x00454E49 // "E2K MACHINE\0"
#define X86_CPUID_VENDOR_VIA           .ebx = 0x20414956, .edx = 0x20414956, .ecx = 0x20414956 // "VIA VIA VIA "
#define X86_CPUID_VENDOR_AO486_OLD     .ebx = 0x756E6547, .edx = 0x41656E69, .ecx = 0x3638344F // "GenuineAO486"
#define X86_CPUID_VENDOR_AO486         .ebx = 0x5453694D, .edx = 0x41207265, .ecx = 0x3638344F // "MiSTer AO486"
#define X86_CPUID_VENDOR_CUSTOM        .ebx = 0x616E6942, .edx = 0x654D7972, .ecx = 0x79646F6C // "BinaryMelody"

#endif // __CPU_H

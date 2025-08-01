
#include "cpu/cpu.h"

#include <assert.h>
#include <inttypes.h>
#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

//// Terminal control

bool kbd_is_init = false;
struct termios kbd_old;

void kbd_reset(void)
{
	if(!kbd_is_init)
		return;
	tcsetattr(0, TCSAFLUSH, &kbd_old);
	kbd_is_init = false;
}

void kbd_init(void)
{
	struct termios kbd_new;
	if(kbd_is_init)
		return;
	tcgetattr(0, &kbd_old);
	kbd_is_init = true;
	atexit(kbd_reset);
	kbd_new = kbd_old;
	cfmakeraw(&kbd_new);
	kbd_new.c_oflag = kbd_old.c_oflag;
	tcsetattr(0, TCSAFLUSH, &kbd_new);
}

int readchar(int fd)
{
	unsigned char c;
	struct pollfd fds[1] = { { fd, POLLIN, 0 } };
	if(poll(fds, 1, 0) <= 0 || (fds[0].revents & POLLIN) == 0)
		return -1;
	if(read(fd, &c, 1) < 1)
		return -1;
	return c;
}

#define KEY_TAB    '\t'
#define KEY_ENTER  '\r'
#define KEY_BS     '\b'
#define KEY_ESC    0x1B

#define KEY_END    0x81
#define KEY_DOWN   0x82
#define KEY_PGDN   0x83
#define KEY_LEFT   0x84
#define KEY_CENTER 0x85
#define KEY_RIGHT  0x86
#define KEY_HOME   0x87
#define KEY_UP     0x88
#define KEY_PGUP   0x89
#define KEY_F1     0x8A
#define KEY_F2     (KEY_F1 - 1 + 2)
#define KEY_F3     (KEY_F1 - 1 + 3)
#define KEY_F4     (KEY_F1 - 1 + 4)
#define KEY_F5     (KEY_F1 - 1 + 5)
#define KEY_F6     (KEY_F1 - 1 + 6)
#define KEY_F7     (KEY_F1 - 1 + 7)
#define KEY_F8     (KEY_F1 - 1 + 8)
#define KEY_F9     (KEY_F1 - 1 + 9)
#define KEY_F10    (KEY_F1 - 1 + 10)
#define KEY_F11    (KEY_F1 - 1 + 11)
#define KEY_F12    (KEY_F1 - 1 + 12)

#define MOD_SHIFT 0x0100
#define MOD_ALT 0x0200
#define MOD_CTRL 0x0400

#define KEY_SHIFT  0x96
#define KEY_ALT    0x97
#define KEY_CTRL   0x98
#define KEY_CAPS   0x99

static int key_map[256] =
{
	['`'] = '`',
	['~'] = '`' | MOD_SHIFT,
	['\0'] = '`' | MOD_CTRL,
	['\x1E'] = '`' | MOD_CTRL | MOD_SHIFT,
	['1'] = '1',
	['!'] = '1' | MOD_SHIFT,
	['2'] = '2',
	['@'] = '2' | MOD_SHIFT,
	['3'] = '3',
	['#'] = '3' | MOD_SHIFT,
	['4'] = '4',
	['$'] = '4' | MOD_SHIFT,
	['5'] = '5',
	['%'] = '5' | MOD_SHIFT,
	['6'] = '6',
	['^'] = '6' | MOD_SHIFT,
	['7'] = '7',
	['&'] = '7' | MOD_SHIFT,
	['8'] = '8',
	['*'] = '8' | MOD_SHIFT,
	['9'] = '9',
	['('] = '9' | MOD_SHIFT,
	['0'] = '0',
	[')'] = '0' | MOD_SHIFT,
	['-'] = '-',
	['_'] = '-' | MOD_SHIFT,
	['='] = '=',
	['+'] = '=' | MOD_SHIFT,
	['\\'] = '\\',
	['|'] = '\\' | MOD_SHIFT,
	['q'] = 'q',
	['Q'] = 'q' | MOD_SHIFT,
	['Q'-0x40] = 'q' | MOD_CTRL,
	['w'] = 'w',
	['W'] = 'w' | MOD_SHIFT,
	['W'-0x40] = 'w' | MOD_CTRL,
	['e'] = 'e',
	['E'] = 'e' | MOD_SHIFT,
	['E'-0x40] = 'e' | MOD_CTRL,
	['r'] = 'r',
	['R'] = 'r' | MOD_SHIFT,
	['R'-0x40] = 'r' | MOD_CTRL,
	['t'] = 't',
	['T'] = 't' | MOD_SHIFT,
	['T'-0x40] = 't' | MOD_CTRL,
	['y'] = 'y',
	['Y'] = 'y' | MOD_SHIFT,
	['Y'-0x40] = 'y' | MOD_CTRL,
	['u'] = 'u',
	['U'] = 'u' | MOD_SHIFT,
	['U'-0x40] = 'u' | MOD_CTRL,
	['i'] = 'i',
	['I'] = 'i' | MOD_SHIFT,
//	['I'-0x40] = 'i' | MOD_CTRL, // tab
	['o'] = 'o',
	['O'] = 'o' | MOD_SHIFT,
	['O'-0x40] = 'o' | MOD_CTRL,
	['p'] = 'p',
	['P'] = 'p' | MOD_SHIFT,
	['P'-0x40] = 'p' | MOD_CTRL,
	['['] = '[',
	['{'] = '[' | MOD_SHIFT,
	[']'] = ']',
	['}'] = ']' | MOD_SHIFT,
	['a'] = 'a',
	['A'] = 'a' | MOD_SHIFT,
	['A'-0x40] = 'a' | MOD_CTRL,
	['s'] = 's',
	['S'] = 's' | MOD_SHIFT,
	['S'-0x40] = 's' | MOD_CTRL,
	['d'] = 'd',
	['D'] = 'd' | MOD_SHIFT,
	['D'-0x40] = 'd' | MOD_CTRL,
	['f'] = 'f',
	['F'] = 'f' | MOD_SHIFT,
	['F'-0x40] = 'f' | MOD_CTRL,
	['g'] = 'g',
	['G'] = 'g' | MOD_SHIFT,
	['G'-0x40] = 'g' | MOD_CTRL,
	['h'] = 'h',
	['H'] = 'h' | MOD_SHIFT,
//	['H'-0x40] = 'h' | MOD_CTRL, // backspace
	['j'] = 'j',
	['J'] = 'j' | MOD_SHIFT,
	['J'-0x40] = 'j' | MOD_CTRL,
	['k'] = 'k',
	['K'] = 'k' | MOD_SHIFT,
	['K'-0x40] = 'k' | MOD_CTRL,
	['l'] = 'l',
	['L'] = 'l' | MOD_SHIFT,
	['L'-0x40] = 'l' | MOD_CTRL,
	[';'] = ';',
	[':'] = ';' | MOD_SHIFT,
	['\''] = '\'',
	['\"'] = '\'' | MOD_SHIFT,
	['z'] = 'z',
	['Z'] = 'z' | MOD_SHIFT,
	['Z'-0x40] = 'z' | MOD_CTRL,
	['x'] = 'x',
	['X'] = 'x' | MOD_SHIFT,
	['X'-0x40] = 'x' | MOD_CTRL,
	['c'] = 'c',
	['C'] = 'c' | MOD_SHIFT,
	['C'-0x40] = 'c' | MOD_CTRL,
	['v'] = 'v',
	['V'] = 'v' | MOD_SHIFT,
	['V'-0x40] = 'v' | MOD_CTRL,
	['b'] = 'b',
	['B'] = 'b' | MOD_SHIFT,
	['B'-0x40] = 'b' | MOD_CTRL,
	['n'] = 'n',
	['N'] = 'n' | MOD_SHIFT,
	['N'-0x40] = 'n' | MOD_CTRL,
	['m'] = 'm',
	['M'] = 'm' | MOD_SHIFT,
//	['M'-0x40] = 'm' | MOD_CTRL, // carriage return
	[','] = ',',
	['<'] = ',' | MOD_SHIFT,
	['.'] = '.',
	['>'] = '.' | MOD_SHIFT,
	['/'] = '/',
	['?'] = '/' | MOD_SHIFT,

	[' '] = ' ',
	['\r'] = '\r',
	['\t'] = '\t',
	['\x7F'] = '\b',
};

static int esc_key_map[256] =
{
	['A'] = KEY_UP,
	['B'] = KEY_DOWN,
	['C'] = KEY_RIGHT,
	['D'] = KEY_LEFT,
	['E'] = KEY_CENTER,
	['F'] = KEY_END,
	['H'] = KEY_HOME,
	['P'] = KEY_F1,
	['Q'] = KEY_F2,
	['R'] = KEY_F3,
	['S'] = KEY_F4,
	['Z'] = '\t' | MOD_CTRL,
};

static int esc_key_map2[] =
{
	[15] = KEY_F5,
	[17] = KEY_F6,
	[18] = KEY_F7,
	[19] = KEY_F8,
	[20] = KEY_F9,
	[21] = KEY_F10,
};

int getkey(int fd)
{
	int c = readchar(fd);
	if(c == 0x1B)
	{
		c = readchar(fd);
		if(c == -1)
			return 0x1B;
		if(c == '[' || c == 'O')
		{
			int c1 = c;
			unsigned params[2] = { 0, 0 };
			unsigned param_count = 0;
			c = readchar(fd);
			if(c != -1)
			{
			next_char:
				switch(c)
				{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					params[param_count] = 10 * params[param_count] + c - '0';
					c = readchar(fd);
					goto next_char;
				case ';':
					if(param_count < 1)
						param_count++;
					c = readchar(fd);
					goto next_char;

				case '~':
					c = params[0] < sizeof esc_key_map2 / sizeof esc_key_map2[0] ? esc_key_map2[params[0]] : 0;
					if(c != 0)
						return c | ((params[1] > 0 ? params[1] - 1 : 0) << 8);
					else
						return 0;

				default:
					c = 0 <= c && c < 256 ? esc_key_map[c] : 0;
					if(c != 0)
						return c | ((params[1] > 0 ? params[1] - 1 : 0) << 8);
					else
						return 0;
				}
			}
			c = c1;
		}
		return 0 <= c && c < 256 ? key_map[c] | MOD_ALT : 0;
	}
	return 0 <= c && c < 256 ? key_map[c] : 0;
}

//// Rudimentary PC emulation

typedef enum x86_pc_type_t
{
	X86_PCTYPE_NONE,
	X86_PCTYPE_IBM_PC_MDA,
	X86_PCTYPE_IBM_PC_CGA,
	X86_PCTYPE_IBM_PCJR,
	X86_PCTYPE_NEC_PC98,
	X86_PCTYPE_NEC_PC88_VA,
	X86_PCTYPE_APRICOT,
	X86_PCTYPE_TANDY2000, // TODO
	X86_PCTYPE_DEC_RAINBOW, // TODO
} x86_pc_type_t;

#define X86_PCTYPE_DEFAULT ((x86_pc_type_t)-1)

static x86_pc_type_t pc_type = X86_PCTYPE_DEFAULT;

enum
{
	X86_IBMPC_IRQ_TIMER = 0,
	X86_IBMPC_IRQ_KEYBOARD = 1,
	X86_IBMPC_IRQ_COM2 = 3,
	X86_IBMPC_IRQ_COM1 = 4,
	X86_IBMPC_IRQ_LPT2 = 5,
	X86_IBMPC_IRQ_FLOPPY = 6,
	X86_IBMPC_IRQ_LPT1 = 7,
	X86_IBMPC_IRQ_CLOCK = 8,
	X86_IBMPC_IRQ_MOUSE = 12,
	X86_IBMPC_IRQ_FPU = 13,
	X86_IBMPC_IRQ_ATA1 = 14,
	X86_IBMPC_IRQ_ATA2 = 15,

	X86_NECPC98_IRQ_TIMER = 0,
	X86_NECPC98_IRQ_KEYBOARD = 1,
	X86_NECPC98_IRQ_CRTV = 2,
	X86_NECPC98_IRQ_RS232C = 4,
	X86_NECPC98_IRQ_PRINTER = 8,
	X86_NECPC98_IRQ_FPU = 14,

	X86_NECPC88VA_V1_IRQ_SERIAL = 0,
	X86_NECPC88VA_V1_IRQ_VRTC = 1,
	X86_NECPC88VA_V1_IRQ_VIDEO = 7,

	X86_NECPC88VA_IRQ_TIMER = 0,
	X86_NECPC88VA_IRQ_KEYBOARD = 1,
	X86_NECPC88VA_IRQ_VRTC = 2,
	X86_NECPC88VA_IRQ_SERIAL = 4,
	X86_NECPC88VA_IRQ_VIDEO = 8,
	X86_NECPC88VA_IRQ_HARDDISK = 9,
	X86_NECPC88VA_IRQ_FLOPPY = 11,
	X86_NECPC88VA_IRQ_SOUND = 12,
	X86_NECPC88VA_IRQ_MOUSE = 13,
	X86_NECPC88VA_IRQ_FPU = 14, // reserved

	X86_APRICOT_IRQ_SINTR1 = 0,
	X86_APRICOT_IRQ_SINTR2 = 1,
	X86_APRICOT_IRQ_FLOPPY = 4,
	X86_APRICOT_IRQ_SERIAL = 5, // also keyboard
	X86_APRICOT_IRQ_TIMER = 6,
	X86_APRICOT_IRQ_FPU = 7,
};

// programmable interrupt controller for IBM PC and NEC PC-98
static struct
{
	bool interrupt_triggered;
	int8_t interrupt_number;
	// interrupt request register: all interrupts that are requesting interrupts
	bool interrupt_requested[8];
	// in-service register: all interrupts that are being serviced
	bool interrupt_serviced[8];
	// routine address for 8080 and interrupt base for 8086 (shifted by 8)
	uint16_t service_routine_address;
	bool i8086_mode;
	// only relevant for 8080
	bool interval4;
	int initialization_word_index;
	bool initialization_word4_needed;
	bool single_mode;
	// TODO: most of these are not used
	bool level_triggered_mode;
	uint8_t device; // for master, a bitmap, for secondary slave, a 3-bit identifier
	bool auto_eoi;
	bool buffered_mode;
	bool master_device;
	bool special_fully_nested_mode;
} i8259[9]; /* master and up to 8 slaves */
static uint8_t i8259_count;

static inline bool i8259_trigger_interrupt(x86_state_t * emu, int device_number)
{
	if(i8259[device_number].i8086_mode)
	{
		if(emu->full_z80_emulation && x86_is_emulation_mode(emu))
			return false;

		return x86_hardware_interrupt(emu, ((i8259[device_number].service_routine_address >> 8) & 0xF8) | (i8259[device_number].interrupt_number & 7), 0, NULL);
	}
	else
	{
		if(!(emu->full_z80_emulation && x86_is_emulation_mode(emu)))
			return false;

		uint8_t call_instruction[3] = { 0xCD, i8259[device_number].service_routine_address >> 8,
			i8259[device_number].interval4
				? (i8259[device_number].service_routine_address & 0xE0) | ((i8259[device_number].interrupt_number & 7) << 2)
				: (i8259[device_number].service_routine_address & 0xC0) | ((i8259[device_number].interrupt_number & 7) << 3) };
		return x86_hardware_interrupt(emu, 0, sizeof call_instruction, call_instruction);
	}
}

// keyboard
static struct
{
	uint8_t buffer[16];
	size_t buffer_length;
	bool data_available;
} i8042;
#define i8251 i8042

static void i8042_pop_buffer(void)
{
	if(i8042.buffer_length == 0)
		return;
	memmove(&i8042.buffer[0], &i8042.buffer[1], i8042.buffer_length - 1);
	i8042.buffer_length--;
}

static void i8042_add_buffer(x86_state_t * emu, uint8_t data)
{
	(void) emu;
	if(i8042.buffer_length >= sizeof i8042.buffer)
		return; // buffer full, ignoring
	if(!i8042.data_available)
		i8042_pop_buffer();
	i8042.buffer[i8042.buffer_length++] = data;
	i8042.data_available = true;
}

static void i8042_acknowledge(void)
{
	if(i8042.buffer_length > 1)
	{
		i8042_pop_buffer();
		i8042.data_available = true;
	}
	else
	{
		i8042.data_available = false;
	}
}

#define i8251_add_buffer i8042_add_buffer
#define i8251_acknowledge i8042_acknowledge

static bool i8259_trigger_interrupts(x86_state_t * emu)
{
	for(int device_number = 0; device_number < i8259_count; device_number++)
	{
		for(int irq_number = 0; irq_number < 8; irq_number++)
		{
			if(i8259[device_number].interrupt_requested[irq_number])
			{
				i8259[device_number].interrupt_serviced[irq_number] = true;
				i8259[device_number].interrupt_requested[irq_number] = false;
				i8259[device_number].interrupt_triggered = true;
				if(device_number != 0)
					i8259[0].interrupt_triggered = true;
				i8259[device_number].interrupt_number = irq_number;
				return i8259_trigger_interrupt(emu, device_number);
			}
		}
	}
	return false;
}

static inline void i8259_send_command(x86_state_t * emu, int device_number, uint8_t command)
{
	(void) emu;
	if((command & 0x10) != 0)
	{
		// initialization word 1
		i8259[device_number].initialization_word_index = 1;
		i8259[device_number].initialization_word4_needed = (command & 0x01) != 0;
		i8259[device_number].single_mode = (command & 0x02) != 0;
		i8259[device_number].interval4 = (command & 0x04) != 0;
		i8259[device_number].level_triggered_mode = (command & 0x08) != 0;
		i8259[device_number].service_routine_address = command & 0xE0;
	}
	else if((command & 0x20) != 0)
	{
		switch(pc_type)
		{
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			i8259[device_number].interrupt_serviced[i8259[device_number].interrupt_number] = false;

			switch(i8259[device_number].interrupt_number)
			{
			case X86_IBMPC_IRQ_KEYBOARD:
				i8042_acknowledge();
				break;
			}
			break;
		case X86_PCTYPE_NEC_PC98:
			switch(i8259[device_number].interrupt_number)
			{
			case X86_NECPC98_IRQ_KEYBOARD:
				i8251_acknowledge();
				break;
			}
			break;
		default:
			break;
		}

		i8259[device_number].interrupt_triggered = false;
	}
	else
	{
		// TODO
	}
}

static inline void i8259_send_data(x86_state_t * emu, int device_number, uint8_t command)
{
	(void) emu;
	if(i8259[device_number].initialization_word_index == 1)
	{
		// initialization word 2
		i8259[device_number].service_routine_address |= command << 8;
	}
	else if(i8259[device_number].initialization_word_index == 2)
	{
		// initialization word 3
		i8259[device_number].device = command;
		// TODO
	}
	else if(i8259[device_number].initialization_word_index == 4)
	{
		// initialization word 4
		i8259[device_number].i8086_mode = (command & 0x01) != 0;
		i8259[device_number].auto_eoi = (command & 0x02) != 0;
		i8259[device_number].master_device = (command & 0x04) != 0;
		i8259[device_number].buffered_mode = (command & 0x08) != 0;
		i8259[device_number].special_fully_nested_mode = (command & 0x10) != 0;
	}
	// TODO

	if(i8259[device_number].initialization_word_index != 0)
	{
		i8259[device_number].initialization_word_index++;
		if(i8259[device_number].initialization_word_index == 3 && i8259[device_number].single_mode)
			i8259[device_number].initialization_word_index++;
		if(i8259[device_number].initialization_word_index == 4 && !i8259[device_number].initialization_word4_needed)
			i8259[device_number].initialization_word_index = 0;
		if(i8259[device_number].initialization_word_index == 5)
			i8259[device_number].initialization_word_index = 0;
	}
}

static bool blinking_enabled = true;

// underline
#define AT_UL "4"

// black, dark gray, light gray, white
#define FG_BK "30"
#define FG_DG "90"
#define FG_LG "37"
#define FG_WH "97"

#define BG_BK "40"
#define BG_DG "100"
#define BG_LG "47"
#define BG_WH "107"

static const char * mda_attribute_table[256] =
{
	BG_BK";"FG_BK, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_BK, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_LG";"FG_BK, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_LG";"FG_DG, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,

	BG_BK";"FG_BK, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_BK, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_BK";"FG_LG, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_BK";"FG_WH, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
	BG_WH";"FG_BK, BG_BK";"FG_LG";"AT_UL, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG, BG_BK";"FG_LG,
		BG_WH";"FG_DG, BG_BK";"FG_WH";"AT_UL, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH, BG_BK";"FG_WH,
};

static int vga_color_table[16] =
{
	0, 4, 2, 6, 1, 5, 3, 7,
	60, 64, 62, 66, 61, 65, 63, 67,
};

static const char * vga_cp437_table[256] =
{
	" ", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂", "♀", "♪", "♫", "☼",
	"►", "◄", "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▲", "▼",
	" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
	"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
	"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "⌂",
	"Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï", "î", "ì", "Ä", "Å",
	"É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ",
	"á", "í", "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»",
	"░", "▒", "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐",
	"└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧",
	"╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█", "▄", "▌", "▐", "▀",
	"α", "ß", "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩",
	"≡", "±", "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", " ",
};

static int necpc98_color_table[8] =
{
	30, 94, 91, 95, 92, 96, 93, 97,
};

#define IBMPC_EXTENDED 0x80
int ibmpc_scancodes[] =
{
	['`'] = 0x29,
	['1'] = 0x02,
	['2'] = 0x03,
	['3'] = 0x04,
	['4'] = 0x05,
	['5'] = 0x06,
	['6'] = 0x07,
	['7'] = 0x08,
	['8'] = 0x09,
	['9'] = 0x0A,
	['0'] = 0x0B,
	['-'] = 0x0C,
	['='] = 0x0D,
	['\b'] = 0x0E,
	['\t'] = 0x0F,
	['q'] = 0x10,
	['w'] = 0x11,
	['e'] = 0x12,
	['r'] = 0x13,
	['t'] = 0x14,
	['y'] = 0x15,
	['u'] = 0x16,
	['i'] = 0x17,
	['o'] = 0x18,
	['p'] = 0x19,
	['['] = 0x1A,
	[']'] = 0x1B,
	['\r'] = 0x1C,
	[KEY_CTRL] = 0x1D, // or 0x1D | IBMPC_EXTENDED
	[KEY_CAPS] = 0x3A,
	['a'] = 0x1E,
	['s'] = 0x1F,
	['d'] = 0x20,
	['f'] = 0x21,
	['g'] = 0x22,
	['h'] = 0x23,
	['j'] = 0x24,
	['k'] = 0x25,
	['l'] = 0x26,
	[';'] = 0x27,
	['\''] = 0x28,
	[KEY_SHIFT] = 0x2A, // or 0x36
	['\\'] = 0x2B,
	['z'] = 0x2C,
	['x'] = 0x2D,
	['c'] = 0x2E,
	['v'] = 0x2F,
	['b'] = 0x30,
	['n'] = 0x31,
	['m'] = 0x32,
	[','] = 0x33,
	['.'] = 0x34,
	['/'] = 0x35,
	[KEY_ALT] = 0x38, // or 0x38 | IBMPC_EXTENDED
	[KEY_HOME] = 0x47,
	[KEY_UP] = 0x48,
	[KEY_PGUP] = 0x49,
	[KEY_LEFT] = 0x4B,
	[KEY_CENTER] = 0x4C,
	[KEY_RIGHT] = 0x4D,
	[KEY_END] = 0x4F,
	[KEY_DOWN] = 0x50,
	[KEY_PGDN] = 0x51,
	[KEY_ESC] = 0x01,
	[KEY_F1] = 0x3B,
	[KEY_F2] = 0x3C,
	[KEY_F3] = 0x3D,
	[KEY_F4] = 0x3E,
	[KEY_F5] = 0x3F,
	[KEY_F6] = 0x40,
	[KEY_F7] = 0x41,
	[KEY_F8] = 0x42,
	[KEY_F9] = 0x43,
	[KEY_F10] = 0x44,
	[KEY_F11] = 0x57,
	[KEY_F12] = 0x58,
	[' '] = 0x39,
};

int necpc98_scancodes[] =
{
	['`'] = 0x00, // same as ESC
	['1'] = 0x01,
	['2'] = 0x02,
	['3'] = 0x03,
	['4'] = 0x04,
	['5'] = 0x05,
	['6'] = 0x06,
	['7'] = 0x07,
	['8'] = 0x08,
	['9'] = 0x09,
	['0'] = 0x0A,
	['-'] = 0x0B,
	['='] = 0x0C,
	['\b'] = 0x0E,
	['\t'] = 0x0F,
	['q'] = 0x10,
	['w'] = 0x11,
	['e'] = 0x12,
	['r'] = 0x13,
	['t'] = 0x14,
	['y'] = 0x15,
	['u'] = 0x16,
	['i'] = 0x17,
	['o'] = 0x18,
	['p'] = 0x19,
	['['] = 0x1A,
	[']'] = 0x1B,
	['\r'] = 0x1C,
	[KEY_CTRL] = 0x74,
	[KEY_CAPS] = 0x71,
	['a'] = 0x1D,
	['s'] = 0x1E,
	['d'] = 0x1F,
	['f'] = 0x20,
	['g'] = 0x21,
	['h'] = 0x22,
	['j'] = 0x23,
	['k'] = 0x24,
	['l'] = 0x25,
	[';'] = 0x26,
	['\''] = 0x27,
	[KEY_SHIFT] = 0x70,
	['\\'] = 0x28,
	['z'] = 0x29,
	['x'] = 0x2A,
	['c'] = 0x2B,
	['v'] = 0x2C,
	['b'] = 0x2D,
	['n'] = 0x2E,
	['m'] = 0x2F,
	[','] = 0x30,
	['.'] = 0x31,
	['/'] = 0x32,
	[KEY_ALT] = 0x73,
	[KEY_HOME] = 0x3E, // or 0x42
	[KEY_UP] = 0x3A, // or 0x43
	[KEY_PGUP] = 0x37, // or 0x44
	[KEY_LEFT] = 0x3B, // or 0x46
	[KEY_CENTER] = 0x47,
	[KEY_RIGHT] = 0x3C, // or 0x48
	[KEY_END] = 0x3F, // or 0x4A
	[KEY_DOWN] = 0x3D, // or 0x4B
	[KEY_PGDN] = 0x36, // or 0x4C
	[KEY_ESC] = 0x00,
	[KEY_F1] = 0x62,
	[KEY_F2] = 0x63,
	[KEY_F3] = 0x64,
	[KEY_F4] = 0x65,
	[KEY_F5] = 0x66,
	[KEY_F6] = 0x67,
	[KEY_F7] = 0x68,
	[KEY_F8] = 0x69,
	[KEY_F9] = 0x6A,
	[KEY_F10] = 0x6B,
	[KEY_F11] = 0x6C,
	[KEY_F12] = 0x6D,
	[' '] = 0x34,
};

static inline void kbd_issue(x86_state_t * emu, int key, bool press)
{
	int scancode;
	switch(pc_type)
	{
	case X86_PCTYPE_IBM_PC_MDA:
	case X86_PCTYPE_IBM_PC_CGA:
	case X86_PCTYPE_IBM_PCJR:
		scancode = ibmpc_scancodes[key];
		if((scancode & IBMPC_EXTENDED) != 0)
		{
			i8042_add_buffer(emu, 0xE0);
			scancode &= ~IBMPC_EXTENDED;
		}
		if(!press)
		{
			scancode |= 0x80;
		}
		i8042_add_buffer(emu, scancode);
		break;
	case X86_PCTYPE_NEC_PC98:
		scancode = necpc98_scancodes[key];
		if(!press)
		{
			scancode |= 0x80;
		}
		i8251_add_buffer(emu, scancode);
		break;
	default:
		break;
	}
}

static bool necpc88va_v3_memory_mode = true;

typedef uint8_t page_t[0x1000];
typedef page_t * page_table_t[0x1000];

static page_table_t _directory;

static inline page_t * _get_page(uaddr_t address)
{
	address >>= 12;
	address &= 0xFFF;
	if(_directory[address] == NULL)
	{
		_directory[address] = malloc(sizeof(page_t));
		memset(_directory[address], 0, sizeof(page_t));
	}
	return _directory[address];
}

static void _memory_read_direct(x86_cpu_level_t memory_space, uaddr_t address, void * buffer, size_t size)
{
	(void) memory_space;
	for(;;)
	{
		page_t * page = _get_page(address);
		uint16_t offset = address & 0xFFF;
		if(offset + size <= 0x1000)
		{
			memcpy(buffer, *page + offset, size);
			break;
		}
		else
		{
			size_t actual_size = 0x1000 - offset;
			memcpy(buffer, *page + offset, actual_size);
			buffer += actual_size;
			size -= actual_size;
			address = (address + actual_size) & ~0xFFF;
		}
	}
}

static void _memory_read(x86_state_t * emu, x86_cpu_level_t memory_space, uaddr_t address, void * buffer, size_t size)
{
	(void) emu;

	switch(pc_type)
	{
	default:
		_memory_read_direct(memory_space, address, buffer, size);
		break;
	case X86_PCTYPE_NEC_PC88_VA:
		if(!necpc88va_v3_memory_mode)
		{
			if(address < 0x1F000)
			{
				size_t actual_size = min(size, 0x1F000 - address);
				_memory_read_direct(memory_space, address + (0xA6000 - 0x1F000), buffer, actual_size);
				if(actual_size == size)
					return;
				buffer += actual_size;
				size -= actual_size;
				address = 0x1F000;
			}
			if(address < 0x20000)
			{
				size_t actual_size = min(size, 0x20000 - address);
				_memory_read_direct(memory_space, address + (0xA6000 - 0x1F000), buffer, size);
				if(actual_size == size)
					return;
				buffer += actual_size;
				size -= actual_size;
				address = 0x20000;
			}
		}
		_memory_read_direct(memory_space, address, buffer, size);
		break;
	}
}

static void _x80_memory_read(x80_state_t * emu, uint16_t address, void * buffer, size_t size)
{
	(void) emu;

	address &= 0xFFFF;
	while(size > 0)
	{
		size_t actual_size = min(0x10000 - address, size);
		_memory_read_direct(X86_LEVEL_USER, address, buffer, size);
		address = 0;
		buffer = (char *)buffer + actual_size;
		size -= actual_size;
	}
}

static void _memory_write_direct(x86_cpu_level_t memory_space, uaddr_t address, const void * buffer, size_t size)
{
	(void) memory_space;
	for(;;)
	{
		page_t * page = _get_page(address);
		uint16_t offset = address & 0xFFF;
		if(offset + size <= 0x1000)
		{
			memcpy(*page + offset, buffer, size);
			break;
		}
		else
		{
			size_t actual_size = 0x1000 - offset;
			memcpy(*page + offset, buffer, actual_size);
			buffer += actual_size;
			size -= actual_size;
			address = (address + actual_size) & ~0xFFF;
		}
	}
}

static void _x80_memory_write(x80_state_t * emu, uint16_t address, const void * buffer, size_t size)
{
	(void) emu;

	address &= 0xFFFF;
	while(size > 0)
	{
		size_t actual_size = min(0x10000 - address, size);
		_memory_write_direct(X86_LEVEL_USER, address, buffer, size);
		address = 0;
		buffer = (const char *)buffer + actual_size;
		size -= actual_size;
	}
}

static bool _screen_printed = false;
static void _display_screen(x86_state_t * emu)
{
	(void) emu;

	_screen_printed = true;

	switch(pc_type)
	{
	case X86_PCTYPE_IBM_PC_MDA:
		{
			uint8_t * memory = *_get_page(0xB0000);
			printf("\33[2J");
			for(int i = 0; i < 25; i++)
			{
				for(int j = 0; j < 80; j++)
				{
					printf("\33[%d;%dH\33[%s%sm%s\33[m", i + 1, j + 1,
						mda_attribute_table[memory[i * 160 + j * 2 + 1] & (blinking_enabled ? 0x7F : 0xFF)],
						blinking_enabled && (memory[i * 160 + j * 2 + 1] & 0x80) != 0 ? ";5" : "",
						vga_cp437_table[memory[i * 160 + j * 2]]);
				}
			}
			printf("\33[26;0H\33[m");
			fflush(stdout);
		}
		break;
	case X86_PCTYPE_IBM_PC_CGA:
	case X86_PCTYPE_IBM_PCJR:
		{
			uint8_t * memory = *_get_page(0xB8000);
			printf("\33[2J");
			for(int i = 0; i < 25; i++)
			{
				for(int j = 0; j < 80; j++)
				{
					printf("\33[%d;%dH\33[%d;%d%sm%s\33[m", i + 1, j + 1,
						30 + vga_color_table[memory[i * 160 + j * 2 + 1] & 0xF],
						40 + vga_color_table[(memory[i * 160 + j * 2 + 1] & (blinking_enabled ? 0x7F : 0xFF)) >> 4],
						blinking_enabled && (memory[i * 160 + j * 2 + 1] & 0x80) != 0 ? ";5" : "",
						vga_cp437_table[memory[i * 160 + j * 2]]);
				}
			}
			printf("\33[26;0H\33[m");
			fflush(stdout);
		}
		break;
	case X86_PCTYPE_NEC_PC98:
		{
			uint8_t * char_memory = *_get_page(0xA0000);
			uint8_t * attr_memory = *_get_page(0xA2000);
			printf("\33[2J");
			for(int i = 0; i < 25; i++)
			{
				for(int j = 0; j < 80; j++)
				{
					printf("\33[%d;%dH\33[%d;40%s%s%sm%s\33[m", i + 1, j + 1,
						necpc98_color_table[(attr_memory[i * 160 + j * 2] >> 5) & 7],
//						(attr_memory[i * 160 + j * 2] & 0x10) != 0 ? "" : "", // TODO: vertical line
						(attr_memory[i * 160 + j * 2] & 0x08) != 0 ? ";4" : "", // underline
						(attr_memory[i * 160 + j * 2] & 0x04) != 0 ? ";7" : "", // reverse
						(attr_memory[i * 160 + j * 2] & 0x02) != 0 ? ";5" : "", // blink
						(attr_memory[i * 160 + j * 2] & 0x01) != 0 // display
							? vga_cp437_table[char_memory[i * 160 + j * 2]]
							: " ");
				}
			}
			printf("\33[26;0H\33[m");
			fflush(stdout);
		}
		break;
	case X86_PCTYPE_NEC_PC88_VA:
		{
			uint8_t * char_memory = *_get_page(0xA6000);
			uint8_t * attr_memory = necpc88va_v3_memory_mode ? *_get_page(0xAE000) : NULL;
			printf("\33[2J");
			for(int i = 0; i < 25; i++)
			{
				for(int j = 0; j < 80; j++)
				{
					uint8_t c, a;
					if(!necpc88va_v3_memory_mode)
					{
						c = char_memory[i * 160 + j * 2];
						a = char_memory[i * 160 + j * 2 + 1];
					}
					else
					{
						c = char_memory[i * 160 + j * 2];
						a = attr_memory[i * 160 + j * 2];
					}

					// TODO: there are other options, for page, for char/attribute layout
					printf("\33[%d;%dH\33[%d;%dm%s\33[m", i + 1, j + 1,
						30 + vga_color_table[a & 0xF],
						40 + vga_color_table[a >> 4],
						vga_cp437_table[c]);
				}
			}
			printf("\33[26;0H\33[m");
			fflush(stdout);
		}
		break;
	case X86_PCTYPE_APRICOT:
		{
			uint16_t * memory = (uint16_t *)*_get_page(0xF0000);
			printf("\33[2J");
			for(int i = 0; i < 25; i++)
			{
				for(int j = 0; j < 80; j++)
				{
					uint16_t value = memory[i * 80 + j];
					uint16_t c;
					if(value < 0x40 || value >= 0x140)
						c = ' ';
					else
						c = value - 0x40;
					printf("\33[%d;%dH\33[%s%s%s%s37;40m%s\33[m", i + 1, j + 1,
						(value & 0x8000) != 0 ? "7;" : "", // reverse
						(value & 0x4000) != 0 ? "1;" : "", // highlight
						(value & 0x2000) != 0 ? "4;" : "", // underline
						(value & 0x1000) != 0 ? "9;" : "", // crossed out
						vga_cp437_table[c]);
				}
			}
			printf("\33[26;0H\33[m");
			fflush(stdout);
		}
		break;
	default:
		break;
	}
}

static void _memory_write_devices(x86_state_t * emu, x86_cpu_level_t memory_space, uaddr_t address, const void * buffer, size_t size)
{
//	fprintf(stderr, "W%X:%d=%02X ...\n", address, size, *(char *)buffer);
	//memcpy(&memory[address & 0xFFFFF], buffer, size);
	_memory_write_direct(memory_space, address, buffer, size);

	switch(pc_type)
	{
	case X86_PCTYPE_IBM_PC_MDA:
		if((address < 0xB0000 && address + size >= 0xB0FA0)
		|| (0xB0000 <= address && address < 0xB0FA0)
		|| (0xB0000 < address + size && address + size <= 0xB0FA0))
		{
			_screen_printed = false;
			_display_screen(emu);
		}
		break;
	case X86_PCTYPE_IBM_PC_CGA:
	case X86_PCTYPE_IBM_PCJR:
		if((address < 0xB8000 && address + size >= 0xB8FA0)
		|| (0xB8000 <= address && address < 0xB8FA0)
		|| (0xB8000 < address + size && address + size <= 0xB8FA0))
		{
			_screen_printed = false;
			_display_screen(emu);
		}
		break;
	case X86_PCTYPE_NEC_PC98:
		if((address < 0xA0000 && address + size >= 0xA0FA0)
		|| (0xA0000 <= address && address < 0xA0FA0)
		|| (0xA0000 < address + size && address + size <= 0xA0FA0)
		|| (address < 0xA2000 && address + size >= 0xA2FA0)
		|| (0xA2000 <= address && address < 0xA2FA0)
		|| (0xA2000 < address + size && address + size <= 0xA2FA0))
		{
			_screen_printed = false;
			_display_screen(emu);
		}
		break;
	case X86_PCTYPE_NEC_PC88_VA:
		if((address < 0xA6000 && address + size >= 0xA6FA0)
		|| (0xA6000 <= address && address < 0xA6FA0)
		|| (0xA6000 < address + size && address + size <= 0xA6FA0)
		|| (address < 0xAE000 && address + size >= 0xAEFA0)
		|| (0xAE000 <= address && address < 0xAEFA0)
		|| (0xAE000 < address + size && address + size <= 0xAEFA0))
		{
			_screen_printed = false;
			_display_screen(emu);
		}
		break;
	case X86_PCTYPE_APRICOT:
		if((address < 0xF0000 && address + size >= 0xF0FA0)
		|| (0xF0000 <= address && address < 0xF0FA0)
		|| (0xF0000 < address + size && address + size <= 0xF0FA0))
		{
			_screen_printed = false;
			_display_screen(emu);
		}
		break;
	default:
		break;
	}
//	printf("%X:%d\n", address, *(uint16_t*)&memory[address & 0xFFFFE]);
}

static void _memory_write(x86_state_t * emu, x86_cpu_level_t memory_space, uaddr_t address, const void * buffer, size_t size)
{
	switch(pc_type)
	{
	default:
		_memory_write_devices(emu, memory_space, address, buffer, size);
		break;
	case X86_PCTYPE_NEC_PC88_VA:
		if(!necpc88va_v3_memory_mode)
		{
			if(address < 0x1F000)
			{
				size_t actual_size = min(size, 0x1F000 - address);
				_memory_write_devices(emu, memory_space, address + (0xA6000 - 0x1F000), buffer, actual_size);
				if(actual_size == size)
					return;
				buffer += actual_size;
				size -= actual_size;
				address = 0x1F000;
			}
			if(address < 0x20000)
			{
				size_t actual_size = min(size, 0x20000 - address);
				_memory_write_devices(emu, memory_space, address + (0xA6000 - 0x1F000), buffer, size);
				if(actual_size == size)
					return;
				buffer += actual_size;
				size -= actual_size;
				address = 0x20000;
			}
		}
		_memory_write_devices(emu, memory_space, address, buffer, size);
		break;
	}
}

static void _port_read(x86_state_t * emu, uint16_t port, void * buffer, size_t count)
{
	(void) emu;
	for(uint16_t offset = 0; offset < count; offset++)
	{
		switch(pc_type)
		{
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			switch(port + offset)
			{
			case 0x0060:
				// 8042 programmable interface data port (keyboard)
				((uint8_t *)buffer)[offset] = i8042.buffer[0];
			}
			break;
		case X86_PCTYPE_NEC_PC98:
			switch(port + offset)
			{
			case 0x0041:
				// 8251 receiver/transmitter (keyboard)
				((uint8_t *)buffer)[offset] = i8251.buffer[0];
			}
			break;
		case X86_PCTYPE_NEC_PC88_VA:
			switch(port + offset)
			{
			case 0x0153:
				// mode select
				((uint8_t *)buffer)[offset] = necpc88va_v3_memory_mode ? 64 : 0;
			}
			break;
		default:
			break;
		}
	}
}

static uint8_t _x80_port_read(x80_state_t * emu, uint16_t port)
{
	(void) emu;
	uint8_t value;
	_port_read(NULL, port, &value, 1);
	return value;
}

static void _port_write(x86_state_t * emu, uint16_t port, const void * buffer, size_t count)
{
	for(uint16_t offset = 0; offset < count; offset++)
	{
		switch(pc_type)
		{
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			switch(port + offset)
			{
			case 0x20:
				// primary 8259 interrupt controller command port
				i8259_send_command(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x21:
				// primary 8259 interrupt controller data port
				i8259_send_data(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			case 0xA0:
				// secondary 8259 interrupt controller command port
				i8259_send_command(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			case 0xA1:
				// secondary 8259 interrupt controller data port
				i8259_send_data(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			}
			break;
		case X86_PCTYPE_NEC_PC98:
			switch(port + offset)
			{
			case 0x0000:
				// primary 8259 interrupt controller command port
				i8259_send_command(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x0002:
				// primary 8259 interrupt controller data port
				i8259_send_data(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x0008:
				// secondary 8259 interrupt controller command port
				i8259_send_command(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x000A:
				// secondary 8259 interrupt controller data port
				i8259_send_data(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			}
			break;
		case X86_PCTYPE_NEC_PC88_VA:
			switch(port + offset)
			{
			case 0x0153:
				// mode select
				necpc88va_v3_memory_mode = (((const uint8_t *)buffer)[offset] & 64) != 0;
				break;
			case 0x0184:
				// secondary 8259 interrupt controller command port
				i8259_send_command(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x0186:
				// secondary 8259 interrupt controller data port
				i8259_send_data(emu, 1, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x0188:
				// primary 8259 interrupt controller command port
				i8259_send_command(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			case 0x018A:
				// primary 8259 interrupt controller data port
				i8259_send_data(emu, 0, ((const uint8_t *)buffer)[offset]);
				break;
			}
			break;
		default:
			break;
		}
	}
}

static void _x80_port_write(x80_state_t * emu, uint16_t port, uint8_t value)
{
	(void) emu;
	_port_write(NULL, port, &value, 1);
}

#include "cpu/x86.list.c"

static inline void setup_x89(x86_state_t * emu, uaddr_t channel_control_block)
{
	emu->x89.present = true;
	emu->x89.initialized = true;
	emu->x89.sysbus = 1; // system bus is 16 bits wide
	emu->x89.soc = 1; // remote bus is 16 bits wide, slave processor

	// set up the channel control blocks at 0x00500, as on the Apricot PC
	emu->x89.cp = channel_control_block;

	for(uint_least8_t channel_number = 0; channel_number < 2; channel_number++)
	{
		x86_memory_write8 (emu, emu->x89.cp + 8 * channel_number,     0); // CCW
		x86_memory_write8 (emu, emu->x89.cp + 8 * channel_number + 1, 0); // busy
		x86_memory_write16(emu, emu->x89.cp + 8 * channel_number + 2, 0); // offset
		x86_memory_write16(emu, emu->x89.cp + 8 * channel_number + 4, 0); // segment
	}

	// TODO: set 8089 I/O port to 0x70 (or 0x72?)
	// TODO: set 8089 interrupt vectors to 0x50, 0x51
}

void machine_setup(x86_state_t * emu, x86_pc_type_t machine)
{
	// keyboard
	if(machine == X86_PCTYPE_IBM_PC_MDA || machine == X86_PCTYPE_IBM_PC_CGA || machine == X86_PCTYPE_IBM_PCJR || machine == X86_PCTYPE_NEC_PC98)
	{
		i8042.buffer[0] = machine == X86_PCTYPE_NEC_PC98 ? 0xFF : 0xAA;
		i8042.buffer_length = 1;
		i8042.data_available = false;
	}
	// TODO: PC88VA, Apricot

	// PIC
	if(machine == X86_PCTYPE_IBM_PC_MDA || machine == X86_PCTYPE_IBM_PC_CGA || machine == X86_PCTYPE_IBM_PCJR || machine == X86_PCTYPE_NEC_PC98 || machine == X86_PCTYPE_NEC_PC88_VA)
	{
		i8259_count = 2;
		for(int device_number = 0; device_number < i8259_count; device_number++)
		{
			i8259[device_number].interrupt_triggered = false;
			switch(machine)
			{
			case X86_PCTYPE_IBM_PC_MDA:
			case X86_PCTYPE_IBM_PC_CGA:
			case X86_PCTYPE_IBM_PCJR:
				i8259[device_number].service_routine_address = (device_number == 0 ? 0x08 : 0x70) << 8;
				break;
			case X86_PCTYPE_NEC_PC98:
			case X86_PCTYPE_NEC_PC88_VA:
				i8259[device_number].service_routine_address = (device_number == 0 ? 0x08 : 0x10) << 8;
				break;
			default:
				break;
			}
			i8259[device_number].i8086_mode = true;
			i8259[device_number].single_mode = false;
			i8259[device_number].level_triggered_mode = false;
		}
	}
	if(machine == X86_PCTYPE_APRICOT)
	{
		i8259_count = 1;
		i8259[0].interrupt_triggered = false;
		i8259[0].service_routine_address = 0x50 << 8;
		i8259[0].i8086_mode = true;
		i8259[0].single_mode = false; // TODO???
		i8259[0].level_triggered_mode = false;
	}

	// 8089
	if(machine == X86_PCTYPE_APRICOT)
	{
		setup_x89(emu, 0x00500);
	}

	// assorted
	switch(pc_type)
	{
	case X86_PCTYPE_IBM_PC_MDA:
		emu->x87.irq_number = X86_IBMPC_IRQ_FPU;

		x86_memory_write8(emu, 0x00449, 0x07);
		x86_memory_write8(emu, 0xFFFF7, '/');
		x86_memory_write8(emu, 0xFFFFA, '/');
		break;
	case X86_PCTYPE_IBM_PC_CGA:
	case X86_PCTYPE_IBM_PCJR: // TODO: how to determine machine type
		emu->x87.irq_number = X86_IBMPC_IRQ_FPU;

		x86_memory_write8(emu, 0x00449, 0x03);
		x86_memory_write8(emu, 0xFFFF7, '/');
		x86_memory_write8(emu, 0xFFFFA, '/');
		break;
	case X86_PCTYPE_NEC_PC98:
		emu->x87.irq_number = X86_NECPC98_IRQ_FPU;
		break;
	case X86_PCTYPE_NEC_PC88_VA:
		emu->x87.irq_number = X86_NECPC88VA_IRQ_FPU;

		necpc88va_v3_memory_mode = !emu->full_z80_emulation;

/*
	INT 0x91
		HL:DE <-> BC
		A[0]: 16-bit
		A[1]: write
		A[2]: I/O
	INT 0x95
		calls x86 routine at 1000:E000, return using IRET
	RETEM
		V1/V2 -> V3, jump to 1000:C003 (not sure what this means)
*/
		break;
	case X86_PCTYPE_APRICOT:
		emu->x87.irq_number = X86_APRICOT_IRQ_FPU;
		break;
	default:
		break;
	}
}

struct load_registers
{
	bool cs_given;
	uoff_t cs;
	bool ss_given;
	uoff_t ss;
	bool ds_given;
	uoff_t ds;
	bool es_given;
	uoff_t es;
	bool ip_given;
	uoff_t ip;
	bool sp_given;
	uoff_t sp;
};

uaddr_t load_bin(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers, size_t maximum)
{
	uaddr_t address;

	if(!registers->cs_given)
	{
		registers->cs_given = true;
		registers->cs = 0;
	}
	if(!registers->ip_given)
	{
		registers->ip_given = true;
		registers->ip = 0;
	}

	address = ((uaddr_t)registers->cs << 4) + registers->ip;

	fseek(input, file_offset, SEEK_SET);
	for(size_t i = 0; i < maximum; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address++, c);
	}

	return address;
}

uaddr_t load_apricot_disk(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	fseek(input, file_offset + 0x0B, SEEK_SET);
	if(fgetc(input) != 1)
	{
		fprintf(stderr, "Error: non-boot disk");
	}

	fseek(input, file_offset + 0x0E, SEEK_SET);
	uint16_t sector_size;
	fread(&sector_size, 2, 1, input);
	sector_size = le16toh(sector_size);

	fseek(input, file_offset + 0x1A, SEEK_SET);
	uint32_t boot_sector_number;
	fread(&boot_sector_number, 4, 1, input);
	boot_sector_number = le32toh(boot_sector_number);
	uint16_t boot_sector_count;
	fread(&boot_sector_count, 2, 1, input);
	boot_sector_count = le16toh(boot_sector_count);

	uint32_t address;
	uint16_t tmp;
	fread(&tmp, 2, 1, input);
	address = le16toh(tmp);

	fread(&tmp, 2, 1, input);
	address += (uint32_t)le16toh(tmp) << 4;

	registers->ip_given = true;
	fread(&tmp, 2, 1, input);
	registers->ip = le16toh(tmp);

	registers->cs_given = true;
	fread(&tmp, 2, 1, input);
	registers->cs = le16toh(tmp);

	fseek(input, file_offset + boot_sector_number * sector_size, SEEK_SET);
	for(unsigned i = 0; i < boot_sector_count * sector_size; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address++, c);
	}

	return address;
}

uaddr_t load_pcjr(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uint16_t tmp;
	fseek(input, file_offset + 0x1CE, SEEK_SET);
	fread(&tmp, 2, 1, input);

	registers->cs_given = true;
	registers->cs = le16toh(tmp);
	registers->ip_given = true;
	registers->ip = 0x0003;

	uaddr_t address = (uaddr_t)registers->cs << 4;

	fseek(input, file_offset + 0x200, SEEK_SET);
	for(int i = 0; ; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address++, c);
	}

	return address;
}

uaddr_t load_com(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uaddr_t address;
	if(!registers->cs_given)
	{
		registers->cs_given = true;
		registers->cs = 0x0100;
	}
	if(!registers->ip_given)
	{
		registers->ip_given = true;
		registers->ip = 0x0100;
	}

	if(!registers->ds_given)
	{
		registers->ds_given = true;
		registers->ds = registers->ss_given ? registers->ss : registers->cs;
	}

	if(!x86_is_emulation_mode(emu))
	{
		if(!registers->ss_given)
		{
			registers->ss_given = true;
			registers->ss = registers->ds;
		}
		else if(registers->ss_given != registers->ds_given)
		{
			fprintf(stderr, "Error: DS != SS\n");
		}
	}
	if(!registers->sp_given)
	{
		registers->sp_given = true;
		registers->sp = 0;
	}

	if(!x86_is_emulation_mode(emu) && !registers->es_given)
	{
		registers->es_given = true;
		registers->es = registers->ds;
	}

	address = ((uaddr_t)registers->cs << 4) + registers->ip;

	if(address < 0x100)
	{
		fprintf(stderr, "Error: program segment prefix does not fit memory\n");
	}

	fseek(input, file_offset, SEEK_SET);
	for(int i = 0; ; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address++, c);
	}

	return address;
}

void load_prl_body(x86_state_t * emu, FILE * input, long file_offset, uoff_t address, uint16_t relocation_address, uint16_t length)
{
	fseek(input, file_offset, SEEK_SET);
	for(uint16_t i = 0; i < length; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address + i, c);
	}
	if(relocation_address != 0x0100)
	{
		for(uint16_t i = 0; i < (length + 7) / 8; i++)
		{
			int c = fgetc(input);
			if(c == -1)
				break;
			for(int j = 7; j >= 0; j--)
			{
				if(((c >> j) & 1) != 0)
				{
					x86_memory_write8(emu, address + i * 8 + 7 - j,
						x86_memory_read8(emu, address + i * 8 + 7 - j) + ((relocation_address - 0x0100) >> 8));
				}
			}
		}
	}
}

uaddr_t load_prl(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uaddr_t address;
	if(!registers->cs_given)
	{
		registers->cs_given = true;
		registers->cs = 0x0100;
	}
	if(!registers->ip_given)
	{
		registers->ip_given = true;
		registers->ip = 0x0100;
	}
	else if((registers->ip & 0xFF) != 0)
	{
		fprintf(stderr, "Error: Invalid load address, IP must be 256-byte page aligned\n");
	}

	address = ((uaddr_t)registers->cs << 4) + registers->ip;

	if(address < 0x100)
	{
		fprintf(stderr, "Error: program segment prefix does not fit memory\n");
	}

	if(!registers->ds_given)
	{
		registers->ds_given = true;
		registers->ds = registers->ss_given ? registers->ss : registers->cs;
	}

	/*if(!registers->ss_given)
	{
		registers->ss_given = true;
		registers->ss = registers->ds;
	}
	else if(registers->ss_given != registers->ds_given)
	{
		fprintf(stderr, "Error: DS != SS\n");
	}*/
	if(!registers->sp_given)
	{
		registers->sp_given = true;
		registers->sp = 0;
	}

	fseek(input, file_offset + 1L, SEEK_SET);
	uint16_t image_size;
	fread(&image_size, 2, 1, input);
	image_size = le16toh(image_size);

	load_prl_body(emu, input, file_offset + 0x100L, address, registers->ip, image_size);

	return address + image_size;
}

uaddr_t load_cpm3(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uaddr_t address;
	if(!registers->cs_given)
	{
		registers->cs_given = true;
		registers->cs = 0x0100;
	}
	if(!registers->ip_given)
	{
		registers->ip_given = true;
		registers->ip = 0x0100;
	}
	else if((registers->ip & 0xFF) != 0)
	{
		fprintf(stderr, "Error: Invalid load address, IP must be 256-byte page aligned\n");
	}

	if(!registers->ds_given)
	{
		registers->ds_given = true;
		registers->ds = registers->ss_given ? registers->ss : registers->cs;
	}

	/*if(!registers->ss_given)
	{
		registers->ss_given = true;
		registers->ss = registers->ds;
	}
	else if(registers->ss_given != registers->ds_given)
	{
		fprintf(stderr, "Error: DS != SS\n");
	}*/

	address = ((uaddr_t)registers->cs << 4) + registers->ip;

	if(address < 0x100)
	{
		fprintf(stderr, "Error: program segment prefix does not fit memory\n");
	}

	fseek(input, file_offset + 1L, SEEK_SET);
	uint16_t image_size;
	fread(&image_size, 2, 1, input);
	image_size = le16toh(image_size);
	// TODO: pre-initialization code is ignored
	fseek(input, file_offset + 0xFL, SEEK_SET);
	uint8_t rsx_count;
	fread(&rsx_count, 1, 1, input);
	rsx_count = min(15, rsx_count);

	fseek(input, file_offset + 0x100L, SEEK_SET);
	for(uint16_t i = 0; i < image_size; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address + i, c);
	}

	struct rsx_record
	{
		uint16_t offset;
		uint16_t length;
	} rsx_records[15];

	for(uint8_t rsx_index = 0; rsx_index < rsx_count; rsx_index++)
	{
		uint16_t tmp;
		fseek(input, file_offset + 0x10L * (1 + rsx_index), SEEK_SET);
		fread(&tmp, 2, 1, input);
		rsx_records[rsx_index].offset = le16toh(tmp);
		fread(&tmp, 2, 1, input);
		rsx_records[rsx_index].length = le16toh(tmp);
	}

	address += image_size;
	for(uint8_t rsx_index = 0; rsx_index < rsx_count; rsx_index++)
	{
		address = (address + 0x1FF) & 0xFF00;
		load_prl_body(emu, input, file_offset + rsx_records[rsx_index].offset, ((uaddr_t)registers->cs << 4) + address, address, rsx_records[rsx_index].length);
		address += 0x100 + rsx_records[rsx_index].length;
	}

	if(!registers->sp_given)
	{
		registers->sp_given = true;
		registers->sp = 0;
	}

	return address;
}

uaddr_t load_exe(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uaddr_t address;

	// Note: the CS:IP values are used to load the image, the actual CS:IP values are read from file
	if(!registers->cs_given)
	{
		registers->cs = registers->ds_given ? registers->ds + 0x0010 : 0x0100;
	}
	if(!registers->ip_given)
	{
		registers->ip = 0x0000;
	}

	address = ((uaddr_t)registers->cs << 4) + registers->ip;

	if(address < 0x100)
	{
		fprintf(stderr, "Error: program segment prefix does not fit memory\n");
	}

	uint16_t tmp;
	fseek(input, file_offset + 2L, SEEK_SET);
	uint32_t image_size;
	fread(&tmp, 2, 1, input);
	image_size = (uint32_t)le16toh(tmp) << 9;
	fread(&tmp, 2, 1, input);
	image_size -= (-le16toh(tmp) & 0x1FF);
	uint16_t relocation_count;
	fread(&relocation_count, 2, 1, input);
	image_size = le16toh(relocation_count);
	fread(&tmp, 2, 1, input);
	uint32_t data_start = (uint32_t)le16toh(tmp) << 4;
	fseek(input, 4L, SEEK_CUR);
	uint16_t sp;
	fseek(input, 2L, SEEK_CUR);
	fread(&sp, 2, 1, input);
	sp = le16toh(sp);
	uint16_t ss;
	fread(&ss, 2, 1, input);
	ss = le16toh(ss);
	fseek(input, 2L, SEEK_CUR);
	uint16_t ip;
	fread(&ip, 2, 1, input);
	ip = le16toh(ip);
	uint16_t cs;
	fread(&cs, 2, 1, input);
	cs = le16toh(cs);
	uint16_t relocation_offset;
	fread(&relocation_offset, 2, 1, input);
	relocation_offset = le16toh(relocation_offset);
	fseek(input, file_offset + data_start, SEEK_SET);
	for(uint16_t i = 0; i < image_size; i++)
	{
		int c = fgetc(input);
		if(c == -1)
			break;
		x86_memory_write8(emu, address + i, c);
	}

	if(relocation_count != 0)
	{
		fseek(input, file_offset + relocation_offset, SEEK_SET);
		uint32_t offset;
		fread(&tmp, 2, 1, input);
		offset = le16toh(tmp);
		fread(&tmp, 2, 1, input);
		offset += (uint16_t)le16toh(tmp) << 4;
		x86_memory_write16(emu, address + offset,
			x86_memory_read16(emu, address + offset) + registers->cs);
	}

	if(registers->ds_given && registers->ds != registers->cs - 0x0010)
	{
		fprintf(stderr, "Error: DS != load segment - 0x0010, overriding\n");
	}
	registers->ds_given = true;
	registers->ds = registers->cs - 0x0010;

	if(!registers->es_given)
	{
		registers->es_given = true;
		registers->es = registers->es;
	}

	if(registers->ss_given && registers->ss != registers->cs + ss)
	{
		fprintf(stderr, "Error: SS != load segment + exe SS, overriding\n");
	}
	registers->ss_given = true;
	registers->ss = registers->cs + ss;

	if(registers->sp_given && registers->sp != sp)
	{
		fprintf(stderr, "Error: SP != exe SP, overriding\n");
	}
	registers->sp_given = true;
	registers->sp = sp;

	registers->cs_given = true;
	registers->cs += cs;
	registers->ip_given = true;
	registers->ip = ip;

	return address + image_size;
}

uaddr_t load_cmd(x86_state_t * emu, FILE * input, long file_offset, struct load_registers * registers)
{
	uaddr_t address;

	struct descriptor
	{
		enum
		{
			INVALID,
			CODE,
			DATA,
			EXTRA,
			STACK,
			PURE_CODE = 9,
		} type;
		uint32_t length;
		uint32_t base;
		uint32_t minimum;
		uint32_t maximum;
	} descriptors[8];
	size_t descriptor_count;

	uint8_t descriptor_indexes[16];
	memset(descriptor_indexes, 0, sizeof descriptor_indexes);

	fseek(input, file_offset, SEEK_SET);

	uint16_t tmp;

	for(descriptor_count = 0; descriptor_count < 8; descriptor_count++)
	{
		uint8_t descriptor_type;
		fread(&descriptor_type, 1, 1, input);
		if(descriptor_type == INVALID)
			break;
		descriptors[descriptor_count].type = descriptor_type;
		fread(&tmp, 2, 1, input);
		descriptors[descriptor_count].length = (uint32_t)le16toh(tmp) << 4; // this will be ignored
		fread(&tmp, 2, 1, input);
		descriptors[descriptor_count].base = (uint32_t)le16toh(tmp) << 4;
		fread(&tmp, 2, 1, input);
		descriptors[descriptor_count].minimum = (uint32_t)le16toh(tmp) << 4;
		fread(&tmp, 2, 1, input);
		descriptors[descriptor_count].maximum = (uint32_t)le16toh(tmp) << 4;

		if(descriptor_indexes[descriptor_type] != 0)
		{
			fprintf(stderr, "Error: duplicated segment type: %d\n", descriptor_type);
		}
		else
		{
			descriptor_indexes[descriptor_type] = descriptor_count + 1;
		}
	}

//	bool tiny_model;
	if(descriptor_count == 0)
	{
		fprintf(stderr, "Invalid file, no segment groups, quitting\n");
		exit(1);
	}
	else if(descriptor_count == 1)
	{
		// tiny model
//		tiny_model = true;

		if(!registers->cs_given)
		{
			registers->cs = 0x0100;
		}
		if(!registers->ip_given)
		{
			registers->ip_given = true;
			registers->ip = 0x0100;
		}
		else if(registers->ip != 0x0100)
		{
			fprintf(stderr, "Error: IP != 0x0100\n");
		}
		address = ((uaddr_t)registers->cs << 4) + registers->ip - 0x0100;
	}
	else
	{
		// small or compact model
//		tiny_model = false;

		if(!registers->cs_given)
		{
			registers->cs = 0x0100;
		}
		if(!registers->ip_given)
		{
			registers->ip_given = true;
			registers->ip = 0x0000;
		}
		else if(registers->ip != 0x0000)
		{
			fprintf(stderr, "Error: IP != 0x0000\n");
		}
		address = ((uaddr_t)registers->cs << 4) + registers->ip;
	}

	if((address & 0xF) != 0)
	{
		fprintf(stderr, "Error: load address is not 8-byte paragraph aligned\n");
	}

	fseek(input, file_offset + 0x7BL, SEEK_SET);
	fread(&tmp, 2, 1, input);
	uint32_t rsx_record_table_offset = (uint32_t)le16toh(tmp) << 7;
	fread(&tmp, 2, 1, input);
	uint32_t relocation_offset = (uint32_t)le16toh(tmp) << 7;
	uint8_t flags;
	fread(&flags, 1, 1, input);

	for(uint8_t descriptor_index = 0; descriptor_index < descriptor_count; descriptor_index++)
	{
		descriptors[descriptor_count].base = address;
		for(uint16_t i = 0; i < descriptors[descriptor_count].length; i++)
		{
			int c = fgetc(input);
			if(c == -1)
				break;
			x86_memory_write8(emu, address + i, c);
		}
		address += descriptors[descriptor_count].maximum;
	}

	if((flags & 0x80) && relocation_offset != 0)
	{
		fseek(input, file_offset + relocation_offset, SEEK_SET);
		uint8_t destination_group;
		fread(&destination_group, 1, 1, input);
		uint8_t source_group = destination_group >> 4;
		destination_group &= 0xF;
		fread(&tmp, 1, 1, input);
		uint32_t offset = (uint32_t)le16toh(tmp) << 4;
		uint8_t byte;
		fread(&byte, 1, 1, input);
		offset += byte;

		if(descriptor_indexes[source_group] != 0 && descriptor_indexes[destination_group] != 0)
		{
			x86_memory_write16(emu, descriptors[descriptor_indexes[source_group] - 1].base + offset,
				x86_memory_read16(emu, descriptors[descriptor_indexes[source_group] - 1].base + offset)
					+ (descriptors[descriptor_indexes[destination_group] - 1].base >> 4));
		}
	}

	uint16_t cs;
	if(descriptor_indexes[CODE] == 0 && descriptor_indexes[PURE_CODE] == 0)
	{
		fprintf(stderr, "Error: no code segment provided\n");
		cs = descriptors[0].base >> 4;
	}
	else if(descriptor_indexes[CODE] != 0 && descriptor_indexes[PURE_CODE] != 0)
	{
		fprintf(stderr, "Error: both code and pure code segment provided\n");
		cs = descriptors[min(descriptor_indexes[CODE ] - 1, descriptor_indexes[PURE_CODE ] - 1)].base >> 4;
	}
	else if(descriptor_indexes[CODE] != 0)
	{
		cs = descriptors[descriptor_indexes[CODE] - 1].base >> 4;
	}
	else
	{
		cs = descriptors[descriptor_indexes[PURE_CODE] - 1].base >> 4;
	}

	registers->cs_given = true;
	registers->cs = cs;

	if(registers->ds_given)
	{
		registers->ds += descriptors[0].base >> 4;
	}
	else
	{
		registers->ds_given = true;
		registers->ds = descriptor_indexes[DATA] != 0 ? descriptors[descriptor_indexes[DATA] - 1].base >> 4 : registers->cs;
	}

	if(registers->es_given)
	{
		registers->es += descriptors[0].base >> 4;
	}
	else
	{
		registers->es_given = true;
		registers->es = descriptor_indexes[EXTRA] != 0 ? descriptors[descriptor_indexes[EXTRA] - 1].base >> 4 : registers->ds;
	}

	if(registers->ss_given)
	{
		registers->ss += descriptors[0].base >> 4;
	}
	else
	{
		registers->ss_given = true;
		registers->ss = address >> 4;
	}

	if(!registers->sp_given)
	{
		registers->sp_given = true;
		registers->sp = 96 - 4;
	}

	if(rsx_record_table_offset != 0)
	{
		uint32_t rsx_records[7];
		uint8_t rsx_count;

		for(rsx_count = 0; rsx_count < 7; rsx_count++)
		{
			uint16_t tmp;
			fseek(input, file_offset + rsx_record_table_offset + 0x10L * rsx_count, SEEK_SET);
			fread(&tmp, 2, 1, input);
			tmp = le16toh(tmp);
			if(tmp == 0xFFFF)
				break;
			rsx_records[rsx_count] = (uint32_t)tmp << 7;
		}

		address += 96;

		for(uint8_t rsx_index = 0; rsx_index < rsx_count; rsx_index++)
		{
			if(rsx_records[rsx_index] == 0)
				continue; // should be loaded from disk

			address = (address + 0xF) & ~0xF;

			struct load_registers _registers;
			memset(&_registers, 0, sizeof _registers);
			_registers.cs_given = true;
			_registers.cs = address >> 4;
			address = load_cmd(emu, input, file_offset + rsx_records[rsx_index], &_registers);
		}
	}

	return address;
}

void usage_cpu(void)
{
	x86_cpu_version_t last_cpu_version = (x86_cpu_version_t)-1;
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"Supported CPUs:\n");
	for(size_t i = 0; i < sizeof supported_cpu_table / sizeof supported_cpu_table[0]; i++)
	{
		if(last_cpu_version != supported_cpu_table[i].type)
		{
			printf("\t%s", supported_cpu_table[i].name);
			last_cpu_version = supported_cpu_table[i].type;
		}
		else
		{
			printf(", %s", supported_cpu_table[i].name);
		}
		if(i == sizeof supported_cpu_table / sizeof supported_cpu_table[0] - 1 || supported_cpu_table[i + 1].type != last_cpu_version)
		{
			printf("\n\t\t%s\n", x86_cpu_traits[last_cpu_version].description);
		}
	}
}

void usage_fpu(void)
{
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"Supported FPUs:\n"
		"\tno, none\n"
		"\t\tno math coprocessor (x87)\n");
	for(size_t i = 0; i < sizeof supported_fpu_table / sizeof supported_fpu_table[0]; i++)
	{
		if(i == 0 || strcmp(supported_fpu_table[i - 1].description, supported_fpu_table[i].description) != 0)
		{
			printf("\t%s", supported_fpu_table[i].name);
		}
		else
		{
			printf(", %s", supported_fpu_table[i].name);
		}
		if(i == sizeof supported_fpu_table / sizeof supported_fpu_table[0] - 1 || strcmp(supported_fpu_table[i].description, supported_fpu_table[i + 1].description) != 0)
		{
			printf("\n\t\t%s\n", supported_fpu_table[i].description);
		}
	}
	printf(
		"\tint, integrated, 487, i487, 80487, i80487\n"
		"\t\tintegrated math coprocessor (x87, x486 or later)\n"
		"Other supported coprocessors:\n"
		"\t89, i89, 8089, i8089\n"
		"\t\tIntel 8089 I/O coprocessor\n"
		"\t80, i80, 8080, i8080, mcs80, mcs-80\n"
		"\t\tIntel 8080, used as secondary CPU\n"
		"\t85, i85, 8085, i8085, mcs85, mcs-80\n"
		"\t\tIntel 8085, used as secondary CPU\n"
		"\tz80\n"
		"\t\tZilog 8085, used as secondary CPU\n");
}

void usage_modes(void)
{
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"Supported modes:\n"
		"\trm16\tbegin execution in 16-bit real mode\n"
		"\tem80\tbegin execution in 8080 emulation mode (V20, µPD9002 only)\n"
		"\tfem80\tbegin execution in full Z80 emulation mode (µPD9002 only)\n"
		"\trm32\tbegin execution in 32-bit real mode, \"big unreal mode\" (requires 80386 or later)\n"
		"\tpm16\tbegin execution in 16-bit protected mode (requires 80286 or later)\n"
		"\tpm32\tbegin execution in 32-bit protected mode (requires 80386 or later)\n"
		"\tcm16\tbegin execution in 16-bit long mode, compatibility mode (requires x86-64)\n"
		"\tcm32\tbegin execution in 32-bit long mode, compatibility mode (requires x86-64)\n"
		"\tlm64\tbegin execution in 64-bit long mode (requires x86-64)\n"
		"\t89, i89, 8089. i8089\n"
		"\t\thalt main CPU and begin execution an 8089 channel program on the first channel\n");
}

void usage_fmts(void)
{
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"Supported formats:\n"
		"\tcom\tMS-DOS or (CP/M-80) .com file loaded at offset 0x100\n"
		"\texe, mz, dos, msdos, ms-dos\n"
		"\t\tMS-DOS .exe file\n"
		"\tcmd, cpm86, cpm-86\n"
		"\t\tCP/M-86 .cmd file\n"
		"\tprl, mpm, mpm80\n"
		"\t\t(8080/8085/Z80) MP/M .prl file\n"
		"\tcpm3, cpm+\n"
		"\t\t(8080/8085/Z80) CP/M Plus .com file\n");
}

void usage_pctype(void)
{
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"Supported PC types:\n"
		"\tno, none\n"
		"\t\tno PC architecture is provided, program will not be able to print to screen or read keyboard input\n"
		"\tmda, herc, hercules\n"
		"\t\tIBM PC with MDA adapter (black and white and high intensity)\n"
		"\tcga, ega, vga, ibmpc\n"
		"\t\tIBM PC with CGA adapter (16 colors)\n"
		"\tpcjr, ibmpcjr\n"
		"\t\tIBM PCjr, support for cartridge loading\n"
		"\tnec, pc98, pc-98, necpc98\n"
		"\t\tNEC PC-98\n"
		"\tpc88, pc-88, pc88va\n"
		"\t\tNEC PC-88 VA, in both V30 and Z80 emulation modes (µPD9002 CPU support)\n"
		"\tapricot, apc\n"
		"\t\tApricot PC (Intel 8089 coprocessor support)\n"
		"\ttandy2000\n"
		"\t\tTandy 2000 (for Tandy 1000, select IBM PCjr)\n"
		"\tdec, decrainbow100, rainbow, rainbow100, decrainbow\n"
		"\t\tDEC Rainbow 100\n");
}

void usage(char * argv0)
{
	printf(
		"x86emu - A versatile x86 emulator and rudimentay PC emulator\n"
		"\tUsage: %s [options] <image file name>\n"
		"\t-c <id>\tCPU type, write -hc to display list of CPUs\n"
		"\t-f <id>\tcoprocessor type, write -hf to display all options\n"
		"\t-m <id>\tstart CPU in mode, write -hm to display all modes\n"
		"\t-l init\tbegin execution at reset location, 0xFFFFFFF0\n"
		"\t-l boot\ttreat image as bootable disk or cartridge (for IBM PCjr)\n"
		"\t-l run\ttreat image as DOS or CP/M executable\n"
		"\t-l <adr>\tloads image at specified address, address can be hexadecimal or a <segment>:<offset> pair of hexadecimal numbers\n"
		"\t-F <fmt>\ttreat image as specific format, write -hF to display list of all supported formats\n"
		"\t-P <type>\tset PC type, write -hP to display list of all supported types\n"
		"\t-O blink\tenable blinking (PC specific)\n"
		"\t-O noblink\tdisable blinking (PC specific)\n"
		"\t-D\tenable disassembly\n"
		"\t-d\tenable single step debugging and disassembly\n"
		"\t-h\tdisplay this help page\n"
		"\t-h <id>\tlist options for a command line flag\n",
		argv0);
}

int main(int argc, char * argv[])
{
	x86_state_t emu[1];
	bool option_debug = false;
	enum
	{
		EXEC_DEFAULT,
		// 16-bit real mode (all CPUs)
		EXEC_RM16,
		// 8080 emulation mode (V20, µPD9002 only)
		EXEC_EM80,
		// 32-bit real mode (386 and later)
		EXEC_RM32,
		// 16-bit protected mode (286 and later)
		EXEC_PM16,
		// 32-bit protected mode (386 and later)
		EXEC_PM32,
		// 16-bit compatibility mode (x86-64 and later)
		EXEC_CM16,
		// 32-bit compatibility mode (x86-64 and later)
		EXEC_CM32,
		// 64-bit long mode (x86-64 and later)
		EXEC_LM64,
		// full Z80 emulation mode (µPD9002 only)
		EXEC_FEM80,
		// 8089 emulation
		EXEC_I89,
	} exec_mode = EXEC_DEFAULT;

	const char * inputfile = NULL;

	memset(emu, 0, sizeof(emu));

	x86_cpu_version_t cpu_version = (x86_cpu_version_t)-1;
	x87_fpu_type_t fpu_type = (x87_fpu_type_t)-1;
	x87_fpu_subtype_t fpu_subtype = 0;

	enum
	{
		LOAD_AUTO,     // attempt to determine, either boot or run
		LOAD_INIT,     // load file at reset position
		LOAD_BOOT,     // load file at boot position
		LOAD_RUN,      // load file at address for executable
		LOAD_SPECIFIC, // load file at specific memory location
	} load_mode = LOAD_AUTO;

	enum
	{
		FMT_AUTO, // determine file format from signature (not always reliable)
		FMT_COM,
		FMT_PRL,
		FMT_EXE,
		FMT_CMD,
		FMT_CPM3,
	} exe_fmt = FMT_AUTO;

	bool load_with_cs = false;
	uoff_t load_cs = 0;
	uoff_t load_ip = 0;

	for(int argi = 1; argi < argc; argi++)
	{
		if(argv[argi][0] == '-')
		{
			if(argv[argi][1] == 'h')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(argi >= argc)
				{
					usage(argv[0]);
				}
				else if(strcmp(arg, "c") == 0 || strcmp(arg, "-c") == 0)
				{
					usage_cpu();
				}
				else if(strcmp(arg, "f") == 0 || strcmp(arg, "-f") == 0)
				{
					usage_fpu();
				}
				else if(strcmp(arg, "m") == 0 || strcmp(arg, "-m") == 0)
				{
					usage_modes();
				}
				else if(strcmp(arg, "F") == 0 || strcmp(arg, "-F") == 0)
				{
					usage_fmts();
				}
				else if(strcmp(arg, "P") == 0 || strcmp(arg, "-P") == 0)
				{
					usage_pctype();
				}
				else
				{
					usage(argv[0]);
				}
				exit(0);
			}
			else if(argv[argi][1] == 'c')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				bool found = false;
				for(size_t i = 0; i < sizeof supported_cpu_table / sizeof supported_cpu_table[0]; i++)
				{
					if(strcmp(supported_cpu_table[i].name, arg) == 0)
					{
						cpu_version = supported_cpu_table[i].type;
						found = true;
						break;
					}
				}
				if(!found)
				{
					fprintf(stderr, "Unknown CPU emulation: %s\n", arg);
					exit(1);
				}
			}
			else if(argv[argi][1] == 'f')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				bool found = false;
				if(strcasecmp(arg, "none") == 0
				|| strcasecmp(arg, "no") == 0)
				{
					fpu_type = X87_FPU_NONE;
					fpu_subtype = 0;
				}
				else if(strcasecmp(arg, "80487") == 0
				|| strcasecmp(arg, "i80487") == 0
				|| strcasecmp(arg, "487") == 0
				|| strcasecmp(arg, "i487") == 0
				|| strcasecmp(arg, "int") == 0
				|| strcasecmp(arg, "integrated") == 0)
				{
					fpu_type = X87_FPU_INTEGRATED;
					fpu_subtype = 0;
				}
				else if(strcasecmp(arg, "8089") == 0
				|| strcasecmp(arg, "i8089") == 0
				|| strcasecmp(arg, "89") == 0
				|| strcasecmp(arg, "i89") == 0)
				{
					emu->x89.present = true;
				}
				else if(strcasecmp(arg, "8080") == 0
				|| strcasecmp(arg, "i8080") == 0
				|| strcasecmp(arg, "80") == 0
				|| strcasecmp(arg, "i80") == 0
				|| strcasecmp(arg, "mcs80") == 0
				|| strcasecmp(arg, "mcs-80") == 0)
				{
					emu->x80.cpu_type = X80_CPU_I80;
					emu->x80.cpu_method = X80_CPUMETHOD_SEPARATE;
				}
				else if(strcasecmp(arg, "8085") == 0
				|| strcasecmp(arg, "i8085") == 0
				|| strcasecmp(arg, "85") == 0
				|| strcasecmp(arg, "i85") == 0
				|| strcasecmp(arg, "mcs85") == 0
				|| strcasecmp(arg, "mcs-85") == 0)
				{
					emu->x80.cpu_type = X80_CPU_I85;
					emu->x80.cpu_method = X80_CPUMETHOD_SEPARATE;
				}
				else if(strcasecmp(arg, "z80") == 0)
				{
					emu->x80.cpu_type = X80_CPU_Z80;
					emu->x80.cpu_method = X80_CPUMETHOD_SEPARATE;
				}
				else
				{
					for(size_t i = 0; i < sizeof supported_fpu_table / sizeof supported_fpu_table[0]; i++)
					{
						if(strcmp(supported_fpu_table[i].name, arg) == 0)
						{
							fpu_type = supported_fpu_table[i].type;
							fpu_subtype = supported_fpu_table[i].subtype;
							found = true;
							break;
						}
					}
					if(!found)
					{
						fprintf(stderr, "Unknown coprocessor emulation: %s\n", arg);
						exit(1);
					}
				}
			}
			else if(argv[argi][1] == 'm')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(strcasecmp(arg, "rm16") == 0)
				{
					exec_mode = EXEC_RM16;
				}
				else if(strcasecmp(arg, "em80") == 0)
				{
					exec_mode = EXEC_EM80;
				}
				else if(strcasecmp(arg, "rm32") == 0)
				{
					exec_mode = EXEC_RM32;
				}
				else if(strcasecmp(arg, "pm16") == 0)
				{
					exec_mode = EXEC_PM16;
				}
				else if(strcasecmp(arg, "pm32") == 0)
				{
					exec_mode = EXEC_PM32;
				}
				else if(strcasecmp(arg, "cm16") == 0)
				{
					exec_mode = EXEC_CM16;
				}
				else if(strcasecmp(arg, "cm32") == 0)
				{
					exec_mode = EXEC_CM32;
				}
				else if(strcasecmp(arg, "lm64") == 0)
				{
					exec_mode = EXEC_LM64;
				}
				else if(strcasecmp(arg, "fem80") == 0)
				{
					exec_mode = EXEC_FEM80;
				}
				else if(strcasecmp(arg, "8089") == 0
				|| strcasecmp(arg, "i8089") == 0
				|| strcasecmp(arg, "89") == 0
				|| strcasecmp(arg, "i89") == 0)
				{
					exec_mode = EXEC_I89;
				}
				else
				{
					fprintf(stderr, "Unknown CPU mode: %s\n", arg);
					exit(1);
				}
			}
			else if(argv[argi][1] == 'l')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(strcasecmp(arg, "init") == 0)
				{
					load_mode = LOAD_INIT;
				}
				else if(strcasecmp(arg, "boot") == 0)
				{
					load_mode = LOAD_BOOT;
				}
				else if(strcasecmp(arg, "run") == 0)
				{
					load_mode = LOAD_RUN;
				}
				else
				{
					load_mode = LOAD_SPECIFIC;
					char * colon = strchr(arg, ':');
					if(colon == NULL)
					{
						load_with_cs = false;
						load_ip = strtoll(arg, NULL, 16);
					}
					else
					{
						*colon = '\0';
						load_with_cs = true;
						load_cs = strtoll(arg, NULL, 16);
						load_ip = strtoll(colon + 1, NULL, 16);
					}
				}
			}
			else if(argv[argi][1] == 'F')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(strcasecmp(arg, "com") == 0)
				{
					exe_fmt = FMT_COM;
				}
				else if(strcasecmp(arg, "exe") == 0
				|| strcasecmp(arg, "mz") == 0
				|| strcasecmp(arg, "dos") == 0
				|| strcasecmp(arg, "msdos") == 0
				|| strcasecmp(arg, "ms-dos") == 0)
				{
					exe_fmt = FMT_EXE;
				}
				else if(strcasecmp(arg, "cmd") == 0
				|| strcasecmp(arg, "cpm86") == 0
				|| strcasecmp(arg, "cpm-86") == 0)
				{
					exe_fmt = FMT_CMD;
				}
				else if(strcasecmp(arg, "prl") == 0
				|| strcasecmp(arg, "mpm") == 0
				|| strcasecmp(arg, "mpm80") == 0)
				{
					exe_fmt = FMT_PRL;
				}
				else if(strcasecmp(arg, "cpm3") == 0
				|| strcasecmp(arg, "cpm+") == 0)
				{
					exe_fmt = FMT_CPM3;
				}
				else
				{
					fprintf(stderr, "Unknown file format: %s\n", arg);
					exit(1);
				}
			}
			else if(argv[argi][1] == 'P')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(strcasecmp(arg, "no") == 0
				|| strcasecmp(arg, "none") == 0)
				{
					pc_type = X86_PCTYPE_NONE;
				}
				else if(strcasecmp(arg, "mda") == 0
				|| strcasecmp(arg, "herc") == 0
				|| strcasecmp(arg, "hercules") == 0)
				{
					pc_type = X86_PCTYPE_IBM_PC_MDA;
				}
				else if(strcasecmp(arg, "cga") == 0
				|| strcasecmp(arg, "ega") == 0
				|| strcasecmp(arg, "vga") == 0
				|| strcasecmp(arg, "ibmpc") == 0)
				{
					pc_type = X86_PCTYPE_IBM_PC_CGA;
				}
				else if(strcasecmp(arg, "pcjr") == 0
				|| strcasecmp(arg, "ibmpcjr") == 0)
				{
					pc_type = X86_PCTYPE_IBM_PCJR;
				}
				else if(strcasecmp(arg, "nec") == 0
				|| strcasecmp(arg, "pc98") == 0
				|| strcasecmp(arg, "pc-98") == 0
				|| strcasecmp(arg, "necpc98") == 0)
				{
					pc_type = X86_PCTYPE_NEC_PC98;
				}
				else if(strcasecmp(arg, "pc88") == 0
				|| strcasecmp(arg, "pc-88") == 0
				|| strcasecmp(arg, "pc88va") == 0)
				{
					pc_type = X86_PCTYPE_NEC_PC88_VA;
				}
				else if(strcasecmp(arg, "apricot") == 0
				|| strcasecmp(arg, "apc") == 0
				|| strcasecmp(arg, "act") == 0)
				{
					pc_type = X86_PCTYPE_APRICOT;
				}
				else if(strcasecmp(arg, "tandy2000") == 0)
				{
					pc_type = X86_PCTYPE_TANDY2000;
				}
				else if(strcasecmp(arg, "dec") == 0
				|| strcasecmp(arg, "decrainbow100") == 0
				|| strcasecmp(arg, "rainbow") == 0
				|| strcasecmp(arg, "rainbow100") == 0
				|| strcasecmp(arg, "decrainbow") == 0)
				{
					pc_type = X86_PCTYPE_DEC_RAINBOW;
				}
				else
				{
					fprintf(stderr, "Unknown machine type: %s\n", arg);
					exit(1);
				}
			}
			else if(argv[argi][1] == 'O')
			{
				char * arg = argv[argi][2] ? &argv[argi][2] : argv[++argi];
				if(strcasecmp(arg, "blink") == 0)
				{
					blinking_enabled = true;
				}
				else if(strcasecmp(arg, "noblink") == 0)
				{
					blinking_enabled = false;
				}
				else
				{
					fprintf(stderr, "Unknown option: %s\n", arg);
					exit(1);
				}
			}
			else if(argv[argi][1] == 'D')
			{
				emu->option_disassemble = true;
			}
			else if(argv[argi][1] == 'd')
			{
				option_debug = emu->option_disassemble = true;
			}
			else
			{
				fprintf(stderr, "Error: Unknown flag `%s`\n", argv[argi]);
				exit(1);
			}
		}
		else
		{
			inputfile = argv[argi];
		}
	}

	if(inputfile == NULL)
	{
		fprintf(stderr, "No input file provided\n");
		exit(1);
	}

	if(pc_type == X86_PCTYPE_DEFAULT)
	{
		pc_type = X86_PCTYPE_IBM_PC_CGA;
		if(cpu_version == (x86_cpu_version_t)-1)
			cpu_version = X86_CPU_TYPE_EXTENDED;
	}

	if(cpu_version == (x86_cpu_version_t)-1)
	{
		switch(pc_type)
		{
		case X86_PCTYPE_NONE:
			cpu_version = X86_CPU_TYPE_EXTENDED;
			break;
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			cpu_version = X86_CPU_TYPE_8088;
			break;
		case X86_PCTYPE_NEC_PC98:
			cpu_version = X86_CPU_TYPE_8086;
			break;
		case X86_PCTYPE_NEC_PC88_VA:
			cpu_version = X86_CPU_TYPE_UPD9002;
			break;
		case X86_PCTYPE_APRICOT:
			cpu_version = X86_CPU_TYPE_8086;
			emu->x89.present = true;
			break;
		case X86_PCTYPE_TANDY2000:
			cpu_version = X86_CPU_TYPE_80186;
			break;
		case X86_PCTYPE_DEC_RAINBOW:
			cpu_version = X86_CPU_TYPE_8088;
			emu->x80.cpu_type = X80_CPU_Z80;
			emu->x80.cpu_method = X80_CPUMETHOD_SEPARATE;
			break;
		}
	}

	emu->cpu_traits = x86_cpu_traits[cpu_version];
	if((emu->cpu_traits.cpuid1.edx & X86_CPUID1_EDX_FPU) != 0)
	{
		emu->x87.fpu_type = X87_FPU_INTEGRATED;
		emu->x87.fpu_subtype = 0;
	}
	else if(fpu_type == X87_FPU_NONE || fpu_type == (x87_fpu_type_t)-1)
	{
		emu->x87.fpu_type = X87_FPU_NONE;
		emu->x87.fpu_subtype = 0;
	}
	else if((emu->cpu_traits.supported_fpu_types & (1 << fpu_type)) != 0)
	{
		emu->x87.fpu_type = fpu_type;
		emu->x87.fpu_subtype = fpu_subtype;
	}
	else
	{
		emu->x87.fpu_type = emu->cpu_traits.default_fpu;
		emu->x87.fpu_subtype = 0;
	}

	emu->memory_read = _memory_read;
	emu->memory_write = _memory_write;
	emu->port_read = _port_read;
	emu->port_write = _port_write;

	emu->parser->use_nec_syntax = x86_is_nec(emu);
	emu->x80.parser->use_intel8080_syntax = emu->cpu_type == X86_CPU_V20;

printf("ok %d\n", emu->x80.cpu_method);

	x86_reset(emu, true);

printf("ok %d\n", emu->x80.cpu_method);

	if(emu->x80.cpu_method == X80_CPUMETHOD_SEPARATE)
	{
		emu->x80.memory_read = emu->x80.memory_fetch = _x80_memory_read;
		emu->x80.memory_write = _x80_memory_write;
		emu->x80.port_read = _x80_port_read;
		emu->x80.port_write = _x80_port_write;

		x80_reset(&emu->x80, true);
	}

	switch(exec_mode)
	{
	case EXEC_DEFAULT:
		// leave state as it was in the beginning
		if(x86_is_emulation_mode(emu))
		{
			exec_mode = emu->full_z80_emulation ? EXEC_FEM80 : EXEC_EM80;
		}
		else if(x86_is_real_mode(emu))
		{
			switch(x86_get_code_size(emu))
			{
			case SIZE_32BIT:
				exec_mode = EXEC_RM32;
				break;
			default:
			case SIZE_16BIT:
				exec_mode = EXEC_RM16;
				break;
			}
		}
		else if(x86_is_long_mode_supported(emu))
		{
			switch(x86_get_code_size(emu))
			{
			default:
			case SIZE_64BIT:
				exec_mode = EXEC_LM64;
				break;
			case SIZE_32BIT:
				exec_mode = EXEC_CM32;
				break;
			case SIZE_16BIT:
				exec_mode = EXEC_CM16;
				break;
			}
		}
		else
		{
			switch(x86_get_code_size(emu))
			{
			default:
			case SIZE_32BIT:
				exec_mode = EXEC_PM32;
				break;
			case SIZE_16BIT:
				exec_mode = EXEC_PM16;
				break;
			}
		}
		break;
	case EXEC_RM16:
		// make the segment 16-bit (286)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access &= ~X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off 8080 emulation mode (v20/9002)
		if(emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002)
			emu->md = X86_FL_MD;
		else if(emu->cpu_type == X86_CPU_EXTENDED)
			emu->md = 0;
		// turn off protected mode (286)
		emu->cr[0] &= ~X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_EM80:
		// make the segment 16-bit (286)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access &= ~X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn on 8080 emulation mode (v20/9002)
		if(emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002)
			emu->md = 0;
		else if(emu->cpu_type == X86_CPU_EXTENDED)
			emu->md = X86_FL_MD;
		emu->md_enabled = true;
		// turn off protected mode (286)
		emu->cr[0] &= ~X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_RM32:
		// make the segment 32-bit (386)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access |= X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off protected mode (286)
		emu->cr[0] &= ~X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_PM16:
		// make the segment 16-bit (286)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access &= ~X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off virtual 8086 mode (386)
		emu->vm = 0;
		// turn on protected mode (286)
		emu->cr[0] |= X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_PM32:
		// make the segment 32-bit (386)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access |= X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off virtual 8086 mode (386)
		emu->vm = 0;
		// turn on protected mode (286)
		emu->cr[0] |= X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_CM16:
		// make the segment 16-bit (286)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access &= ~X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off virtual 8086 mode (386)
		emu->vm = 0;
		// turn on protected mode (286)
		emu->cr[0] |= X86_CR0_PE;
		// turn on long mode (x64)
		emu->efer |= X86_EFER_LMA | X86_EFER_LME;
		break;
	case EXEC_CM32:
		// make the segment 32-bit (386)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access |= X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn off virtual 8086 mode (386)
		emu->vm = 0;
		// turn on protected mode (286)
		emu->cr[0] |= X86_CR0_PE;
		// turn on long mode (x64)
		emu->efer |= X86_EFER_LMA | X86_EFER_LME;
		break;
	case EXEC_LM64:
		// make the segment 64-bit (x64)
		emu->sr[X86_R_CS].access &= ~X86_DESC_D;
		emu->sr[X86_R_CS].access |= X86_DESC_L;
		// turn off virtual 8086 mode (386)
		emu->vm = 0;
		// turn on protected mode (286)
		emu->cr[0] |= X86_CR0_PE;
		// turn on long mode (x64)
		emu->efer |= X86_EFER_LMA | X86_EFER_LME;
		break;
	case EXEC_FEM80:
		// make the segment 16-bit (286)
		for(x86_segnum_t segnum = X86_R_ES; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].access &= ~X86_DESC_D;
		}
		emu->sr[X86_R_CS].access &= ~X86_DESC_L;
		// turn on 8080 emulation mode (v20/9002)
		if(emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_UPD9002)
			emu->md = 0;
		else if(emu->cpu_type == X86_CPU_EXTENDED)
			emu->md = X86_FL_MD;
		emu->md_enabled = true;
		emu->full_z80_emulation = true;
		// turn off protected mode (286)
		emu->cr[0] &= ~X86_CR0_PE;
		// turn off long mode (x64)
		emu->efer &= ~(X86_EFER_LMA | X86_EFER_LME);
		break;
	case EXEC_I89:
		setup_x89(emu, 0x00500);

		// place CPU in infinite loop
		emu->xip = 0;
		emu->sr[X86_R_CS].selector = 0x40;
		emu->sr[X86_R_CS].base = 0x40 << 4;
		x86_memory_write8_external(emu, 0x400, 0xF4); // hlt
		x86_memory_write8_external(emu, 0x401, 0xEB); // jmp
		x86_memory_write8_external(emu, 0x402, 0xFD); // -3
		break;
	}

	FILE * input;

	input = fopen(inputfile, "rb");

	if(input == NULL)
	{
		fprintf(stderr, "Invalid input file %s\n", inputfile);
		exit(1);
	}

	struct load_registers registers;
	memset(&registers, 0, sizeof registers);

	switch(load_mode)
	{
	case LOAD_AUTO:
		if(exec_mode == EXEC_I89)
		{
			registers.ip_given = true;
			registers.ip = 0x1000;
			registers.cs_given = true;
			registers.cs = 0x0000;

			load_bin(emu, input, 0, &registers, (size_t)-1);
		}
		else
		{
			if(strlen(inputfile) >= 3)
			{
				if(strcasecmp(inputfile + strlen(inputfile) - 4, ".com") == 0
				|| ((exec_mode == EXEC_EM80 || exec_mode == EXEC_FEM80) && strcasecmp(inputfile + strlen(inputfile) - 4, ".prl") == 0)
				|| (!(exec_mode == EXEC_EM80 || exec_mode == EXEC_FEM80) && strcasecmp(inputfile + strlen(inputfile) - 4, ".exe") == 0)
				|| (!(exec_mode == EXEC_EM80 || exec_mode == EXEC_FEM80) && strcasecmp(inputfile + strlen(inputfile) - 4, ".cmd") == 0))
				{
					load_mode = LOAD_RUN;
					goto case_load_run;
				}
			}
			load_mode = LOAD_BOOT;
			goto case_load_boot;
		}
		break;

	case LOAD_INIT:
		if(exec_mode == EXEC_EM80 || exec_mode == EXEC_FEM80)
		{
			registers.ip_given = true;
			registers.ip = 0x0000;
			registers.cs_given = true;
			registers.cs = 0x0000;
		}
		else
		{
			// otherwise, defaults
			registers.ip_given = true;
			registers.ip = emu->xip;
			registers.cs_given = true;
			registers.cs = emu->sr[X86_R_CS].base >> 4;
		}
		// TODO
		break;

	case LOAD_BOOT:
	case_load_boot:
		if(pc_type == X86_PCTYPE_DEFAULT || pc_type == X86_PCTYPE_IBM_PCJR)
		{
			char signature[4];
			if(fread(signature, 1, 4, input) == 4 && memcmp(signature, "PCjr", 4) == 0)
			{
				if(pc_type == X86_PCTYPE_DEFAULT)
					pc_type = X86_PCTYPE_IBM_PCJR;
				load_pcjr(emu, input, 0, &registers);
				break;
			}
		}

		if(pc_type == X86_PCTYPE_DEFAULT)
		{
			pc_type = X86_PCTYPE_IBM_PC_CGA;
		}

		switch(pc_type)
		{
		case X86_PCTYPE_NONE:
			load_bin(emu, input, 0, &registers, (size_t)-1);
			break;
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			registers.ip_given = true;
			registers.ip = 0x7C00;
			registers.cs_given = true;
			registers.cs = 0x0000;
			load_bin(emu, input, 0, &registers, 0x200);
			break;
		case X86_PCTYPE_NEC_PC98:
			registers.ip_given = true;
			registers.ip = 0x0000;
			registers.cs_given = true;
			registers.cs = 0x1FE0; // TODO: sometimes 0x1FC0
			load_bin(emu, input, 0, &registers, 0x200);
			break;
		case X86_PCTYPE_NEC_PC88_VA:
			registers.ip_given = true;
			registers.cs_given = true;
			registers.sp_given = true;
			registers.ss_given = true;
			if(exec_mode != EXEC_FEM80)
			{
				registers.ip = 0x0000;
				registers.cs = 0x3000;
				registers.sp = 0xFFFE;
				registers.ss = 0x3000;
				load_bin(emu, input, 0, &registers, 0x200);
			}
			else
			{
				registers.ip = 0x3000;
				registers.cs = 0x1000;
				registers.sp = 0xFFFE;
				registers.ss = 0x1000;
				load_bin(emu, input, 0, &registers, 0x100);
			}
			break;
		case X86_PCTYPE_APRICOT:
			// loader can figure out the starting address
			load_apricot_disk(emu, input, 0, &registers);
			break;
		case X86_PCTYPE_TANDY2000:
			fprintf(stderr, "Tandy 2000 not yet implemented\n");
			exit(1);
		case X86_PCTYPE_DEC_RAINBOW:
//			fprintf(stderr, "DEC Rainbow not yet implemented\n");
//			exit(1);
				load_bin(emu, input, 0, &registers, 0x100);
break;
		}
		break;

	case LOAD_SPECIFIC:
		registers.ip_given = true;
		registers.ip = load_ip;
		if(load_with_cs)
		{
			registers.cs_given = true;
			registers.cs = load_cs;
		}
		__attribute__((fallthrough));

	case LOAD_RUN:
	case_load_run:
		switch(exe_fmt)
		{
		case FMT_AUTO:
			if(exec_mode == EXEC_EM80 || exec_mode == EXEC_FEM80)
			{
				int c = fgetc(input);
				if(c == 0)
				{
					exe_fmt = FMT_PRL;
					goto case_fmt_prl;
				}
				else if((c & 0xFF) == 0xC3)
				{
					exe_fmt = FMT_CPM3;
					goto case_fmt_cpm3;
				}
				else
				{
					exe_fmt = FMT_COM;
					goto case_fmt_com;
				}
			}
			else
			{
				uint8_t magic[2];
				fread(magic, 2, 1, input);
				if((magic[0] == 'M' && magic[2] == 'Z')
				|| (magic[0] == 'Z' && magic[2] == 'M'))
				{
					exe_fmt = FMT_EXE;
					goto case_fmt_exe;
				}
				else
				{
					exe_fmt = FMT_COM;
					goto case_fmt_com;
				}
			}
			break;

		case FMT_COM:
		case_fmt_com:
			load_com(emu, input, 0, &registers);
			break;
		case FMT_PRL:
		case_fmt_prl:
			load_prl(emu, input, 0, &registers);
			break;
		case FMT_CPM3:
		case_fmt_cpm3:
			load_cpm3(emu, input, 0, &registers);
			break;
		case FMT_EXE:
		case_fmt_exe:
			load_exe(emu, input, 0, &registers);
			break;
		case FMT_CMD:
			load_cmd(emu, input, 0, &registers);
			break;
		default:
			break;
		}
	}

	if(pc_type == X86_PCTYPE_DEFAULT)
	{
		pc_type = X86_PCTYPE_IBM_PC_CGA;
	}

	machine_setup(emu, pc_type);

	uint16_t selector;

	switch(exec_mode)
	{
	case EXEC_DEFAULT:
		assert(false); // already changed
	case EXEC_RM16:
	case EXEC_RM32:
		if(!registers.cs_given)
		{
			registers.cs = (registers.ip >> 4) & ~0x0FFF;
			registers.ip &= 0xFFFF;
		}

		emu->xip = registers.ip & 0xFFFF;
		emu->sr[X86_R_CS].selector = registers.cs;
		emu->sr[X86_R_CS].base = registers.cs << 4;
		if(!registers.ss_given)
			registers.ss = registers.ds_given ? registers.ds : registers.cs;
		emu->sr[X86_R_SS].selector = registers.ss;
		emu->sr[X86_R_SS].base = registers.ss << 4;
		if(!registers.ds_given)
			registers.ds = registers.ss_given ? registers.ss : registers.cs;
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = X86_R_DS3 <= segnum ? registers.ds >> 4 : registers.ds;
			emu->sr[segnum].base = registers.ds << 4;
		}
		if(registers.sp_given)
			emu->gpr[X86_R_SP] = registers.sp;
		break;

	case EXEC_EM80:
	case EXEC_FEM80:
		if(!registers.cs_given)
		{
			registers.cs = (registers.ip >> 4) & 0xF000;
			registers.ip &= 0xFFFF;
		}

		emu->x80.pc = emu->xip = registers.ip & 0xFFFF;
		emu->sr[X86_R_CS].selector = registers.cs;
		emu->sr[X86_R_CS].base = registers.cs << 4;
		if(!registers.ss_given)
			registers.ss = registers.ds_given ? registers.ds : registers.cs;
		emu->sr[X86_R_SS].selector = registers.ss;
		emu->sr[X86_R_SS].base = registers.ss << 4;
		if(!registers.ds_given)
			registers.ds = registers.ss_given ? registers.ss : registers.cs;
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = X86_R_DS3 <= segnum ? registers.ds >> 4 : registers.ds;
			emu->sr[segnum].base = registers.ds << 4;
		}
		if(registers.sp_given)
			emu->x80.sp = emu->gpr[X86_R_BP] = registers.sp;
		break;

		if(!registers.cs_given)
		{
			registers.cs = (registers.ip >> 4) & 0xF000;
			registers.ip &= 0xFFFF;
		}

		emu->xip = registers.ip & 0xFFFF;
		emu->sr[X86_R_CS].selector = registers.cs;
		emu->sr[X86_R_CS].base = registers.cs << 4;
		if(!registers.ss_given)
			registers.ss = registers.ds_given ? registers.ds : registers.cs;
		emu->sr[X86_R_SS].selector = registers.ss;
		emu->sr[X86_R_SS].base = registers.ss << 4;
		if(!registers.ds_given)
			registers.ds = registers.ss_given ? registers.ss : registers.cs;
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = registers.ds;
			emu->sr[segnum].base = registers.ds << 4;
		}
		if(registers.sp_given)
			emu->gpr[X86_R_SP] = registers.sp;
		break;

	case EXEC_PM16:
	case EXEC_CM16:
		if(!registers.cs_given)
		{
			registers.cs = 0x1000;
		}

		emu->xip = registers.ip & 0xFFFF;
		emu->sr[X86_R_CS].selector = 0x08;
		emu->sr[X86_R_CS].base = registers.cs << 4;
		if(!registers.ss_given)
			registers.ss = registers.ds_given ? registers.ds : registers.cs;
		emu->sr[X86_R_SS].selector = 0x10;
		emu->sr[X86_R_SS].base = registers.ss << 4;
		if(!registers.ds_given)
			registers.ds = registers.ss_given ? registers.ss : registers.cs;
		selector = registers.ss_given && registers.ds_given ? 0x18 : 0x10;
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = selector;
			emu->sr[segnum].base = registers.ds << 4;
		}
		if(registers.sp_given)
			emu->gpr[X86_R_SP] = registers.sp;
		break;

	case EXEC_PM32:
	case EXEC_CM32:
		if(!registers.cs_given)
		{
			registers.cs = 0;
		}

		emu->xip = registers.ip & 0xFFFFFFFF;
		emu->sr[X86_R_CS].selector = 0x08;
		emu->sr[X86_R_CS].base = registers.cs << 4;
		if(!registers.ss_given)
			registers.ss = registers.ds_given ? registers.ds : registers.cs;
		emu->sr[X86_R_SS].selector = 0x10;
		emu->sr[X86_R_SS].base = registers.ss << 4;
		if(!registers.ds_given)
			registers.ds = registers.ss_given ? registers.ss : registers.cs;
		selector = registers.ss_given && registers.ds_given ? 0x18 : 0x10;
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = selector;
			emu->sr[segnum].base = registers.ds << 4;
		}
		if(registers.sp_given)
			emu->gpr[X86_R_SP] = registers.sp;
		break;

	case EXEC_LM64:
		if(registers.cs_given)
		{
			registers.ip += registers.cs;
		}

		if(registers.ss_given)
		{
			registers.sp += registers.ss;
		}

		emu->xip = registers.ip + (registers.cs_given ? registers.cs << 4 : 0);
		emu->sr[X86_R_CS].selector = 0x08;
		emu->sr[X86_R_SS].selector = 0x10 + (registers.ss_given ? registers.ss << 4 : 0);
		for(x86_segnum_t segnum = X86_R_DS; segnum <= X86_R_DS2; segnum++)
		{
			emu->sr[segnum].selector = 0x10;
		}
		if(registers.sp_given)
			emu->gpr[X86_R_SP] = registers.sp;
		break;

	case EXEC_I89:
		if(!registers.cs_given)
		{
			registers.cs = (registers.ip >> 4) & ~0x0FFF;
			registers.ip &= 0xFFFF;
		}

		x86_memory_write8_external (emu, emu->x89.cp + 8 * 0,     0x13); // CCW (start in system space)
		x86_memory_write8_external (emu, emu->x89.cp + 8 * 0 + 1, 0xFF); // busy (running)
		x86_memory_write16_external(emu, emu->x89.cp + 8 * 0 + 2, emu->x89.cp + 0x10); // offset
		x86_memory_write16_external(emu, emu->x89.cp + 8 * 0 + 4, 0x0000); // segment

		x86_memory_write16_external(emu, emu->x89.cp + 0x10, registers.ip); // offset
		x86_memory_write16_external(emu, emu->x89.cp + 0x12, registers.cs); // segment

		emu->x89.channel[0].r[X89_R_TP].address = ((uint32_t)registers.cs << 4) + registers.ip;
		emu->x89.channel[0].r[X89_R_TP].tag = 0;
		emu->x89.channel[0].pp = emu->x89.cp + 0x10;
		emu->x89.channel[0].psw = 0x80; // interrupts enabled
		emu->x89.channel[0].running = true;
		break;
	}

	fclose(input);

	if(!option_debug)
	{
		kbd_init();
	}

	_display_screen(emu);

	bool continuous = false;
	uint64_t breakpoint = 0;
	while(true)
	{
		if(option_debug && !continuous && emu->xip >= breakpoint)
		{
			breakpoint = 0;
			x86_debug(stderr, emu);
			if(emu->x80.cpu_method == X80_CPUMETHOD_SEPARATE)
				x80_debug(stderr, &emu->x80);
			if(emu->cpu_traits.prefetch_queue_size > 0)
			{
				// TODO: prefetch queue should be filled after the instruction has been parsed, but before it is executed
				// unfortunately, this cannot be done together with the way string instructions are implemented (by resetting IP to the start each time)
				fprintf(stderr, "Bytes left in queue: %d, IP=%08lX, prefetch: %08lX",
					emu->prefetch_queue_data_size, emu->xip, emu->prefetch_pointer);
				for(uint8_t pointer = 0; pointer < emu->prefetch_queue_data_size; pointer++)
					fprintf(stderr, " %02X", emu->prefetch_queue[emu->prefetch_queue_data_offset + pointer]);
				fprintf(stderr, "\n");
			}
//			printf("%X\n", x86_memory_segmented_read16(emu, X86_R_CS, emu->xip));
			if(option_debug && !continuous)
			{
				int c = getchar();
				switch(c)
				{
				case 'q':
					fprintf(stderr, "Quitting\n");
					exit(0);
				case 's':
					breakpoint = emu->old_xip + 1;
					fprintf(stderr, "Skipping to %"PRIX64"\n", breakpoint);
					break;
				case 'r':
					fprintf(stderr, "Running until next interrupt\n");
					continuous = true;
					kbd_init();
					break;
				}
			}
		}

		if(option_debug)
		{
			emu->parser->debug_output[0] = '\0';
			fprintf(stderr, "Testing disassembly: <<<\n[X86]\t");
			uoff_t old_position = emu->parser->current_position;
			x86_disassemble(emu->parser, emu);
			emu->parser->current_position = old_position;
			fprintf(stderr, "%s", emu->parser->debug_output);
			if(emu->x89.present)
			{
				emu->parser->debug_output[0] = '\0';

				for(unsigned channel_number = 0; channel_number < 2; channel_number++)
				{
					if(emu->x89.channel[channel_number].running)
					{
						fprintf(stderr, "[X89:%d]\t", channel_number);
						x89_parser_t * prs = x89_setup_parser(emu, channel_number);
						x89_disassemble(prs);
						fprintf(stderr, "%s", prs->debug_output);
						free(prs);
					}
				}
				fprintf(stderr, "\n");
			}
			if(emu->x80.cpu_method == X80_CPUMETHOD_SEPARATE)
			{
				emu->x80.parser->debug_output[0] = '\0';
				fprintf(stderr, "[X80]\t");
				x80_disassemble(emu->x80.parser);
				fprintf(stderr, "%s", emu->x80.parser->debug_output);
				fprintf(stderr, "\n");
			}
			fprintf(stderr, ">>>\n");
		}

		bool inhibit_interrupts = false;

		_screen_printed = false;
		emu->parser->debug_output[0] = '\0';
		x86_result_t result = x86_step(emu);
		switch(X86_RESULT_TYPE(result))
		{
		case X86_RESULT_SUCCESS:
			break;
		case X86_RESULT_STRING:
			break;
		case X86_RESULT_HALT:
//			fprintf(stderr, "CPU halted\n");
			break;
		case X86_RESULT_CPU_INTERRUPT:
			break;
		case X86_RESULT_ICE_INTERRUPT:
			break;
		case X86_RESULT_IRQ:
			i8259[X86_RESULT_VALUE(result) >> 3].interrupt_requested[X86_RESULT_VALUE(result) & 7] = true;
			break;
		case X86_RESULT_TRIPLE_FAULT:
			fprintf(stderr, "Triple fault\n");
			break;
		case X86_RESULT_INHIBIT_INTERRUPTS:
			inhibit_interrupts = true;
			break;
		}
		x87_step(emu);
		x89_step(emu);
		if(emu->x80.cpu_method == X80_CPUMETHOD_SEPARATE)
		{
			x80_step(&emu->x80, NULL);
		}

		if(option_debug && !continuous && breakpoint == 0 && !_screen_printed)
			_display_screen(emu);
		fprintf(stderr, "%s", emu->parser->debug_output);

		if(!(option_debug && !continuous))
		{
			int key = getkey(0);
			if(key == ('c' | MOD_CTRL))
			{
				fprintf(stderr, "Press ESC twice to quit\n");
			}
			else if(key == KEY_ESC)
			{
				fprintf(stderr, "If you want to quit, press ESC again\n");
				int key2;
				while((key2 = getkey(0)) == 0)
					;
				if(key2 == KEY_ESC)
				{
					fprintf(stderr, "Quitting\n");
					exit(0);
				}
				fprintf(stderr, "You pressed %d, continuing\n", key2);
			}
			if(key != 0)
			{
//fprintf(stderr, "[%04X]\n", key);
				if((key & MOD_SHIFT))
					kbd_issue(emu, KEY_SHIFT, true);
				if((key & MOD_CTRL))
					kbd_issue(emu, KEY_CTRL, true);
				if((key & MOD_ALT))
					kbd_issue(emu, KEY_ALT, true);
				kbd_issue(emu, key & 0xFF, true);
				kbd_issue(emu, key & 0xFF, false);
				if((key & MOD_ALT))
					kbd_issue(emu, KEY_ALT, false);
				if((key & MOD_CTRL))
					kbd_issue(emu, KEY_CTRL, false);
				if((key & MOD_SHIFT))
					kbd_issue(emu, KEY_SHIFT, false);
			}
		}

		switch(pc_type)
		{
		case X86_PCTYPE_IBM_PC_MDA:
		case X86_PCTYPE_IBM_PC_CGA:
		case X86_PCTYPE_IBM_PCJR:
			if(i8042.data_available)
			{
				i8259[X86_IBMPC_IRQ_KEYBOARD >> 3].interrupt_requested[X86_IBMPC_IRQ_KEYBOARD & 7] = true;
			}
			// TODO: other peripherals
			break;
		case X86_PCTYPE_NEC_PC98:
			if(i8042.data_available)
			{
				i8259[X86_NECPC98_IRQ_KEYBOARD >> 3].interrupt_requested[X86_NECPC98_IRQ_KEYBOARD & 7] = true;
			}
			// TODO: other peripherals
			break;
		case X86_PCTYPE_APRICOT:
			if((emu->x89.channel[0].psw & X89_PSW_IS) != 0)
			{
				// TODO: make the 8089 interrupts available on non-Apricot machines; issue: which IRQ?
				// TODO: should "acknowledge" clear this bit?
				i8259[X86_APRICOT_IRQ_SINTR1 >> 3].interrupt_requested[X86_APRICOT_IRQ_SINTR1 & 7] = true;
			}
			if((emu->x89.channel[1].psw & X89_PSW_IS) != 0)
			{
				// TODO: same as channel 0
				i8259[X86_APRICOT_IRQ_SINTR2 >> 3].interrupt_requested[X86_APRICOT_IRQ_SINTR2 & 7] = true;
			}
			// TODO: other peripherals
			break;
		// TODO: other PCs
		default:
			break;
		}

		if(!inhibit_interrupts)
		{
			bool triggered = i8259_trigger_interrupts(emu);
			if(option_debug && triggered)
			{
				continuous = false;
				kbd_reset();
			}
		}
	}

	return 0;
}



#include "system.h"

void start()
{
__asm;
	ld	hl, 4
	add	hl, sp
	push	hl
	call	_main
	push	hl
	call	_exit
__endasm;
#define DEFSYSCALL(name, number) \
_##name: \
	ld	hl, number \
	push	hl \
	rst	0x30 \
	inc	sp \
	inc	sp \
	ret
__asm;
	DEFSYSCALL(exit, 0)
	DEFSYSCALL(write, 8)
__endasm;
}

size_t strlen(const char * s)
{
	size_t length;
	for(length = 0; s[length] != '\0'; length++)
		;
	return length;
}

void putstr(const char * s)
{
	write(1, s, strlen(s));
}

void putint(unsigned int value)
{
	char buffer[(sizeof(int) * 5 + 1) / 2];
	int pointer = sizeof buffer;
	do
	{
		buffer[--pointer] = '0' + (value % 10);
		value /= 10;
	} while(value != 0);
	write(1, &buffer[pointer], sizeof buffer - pointer);
}


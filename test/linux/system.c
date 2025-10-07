
#include "system.h"

asm(
	".global\t_start\n"
	"_start:\n"
#if __ia16__
	"\tmovw\t%sp, %bp\n"
	"\tmovw\t(%bp), %ax\n"
	"\tleaw\t2(%bp), %bx\n"
	"\tmovw\t%ax, %si\n"
	"\tshlw\t$1, %si\n"
	"\tleaw\t4(%bp,%si), %si\n"
	"\tpushw\t%si\n"
	"\tpushw\t%bx\n"
	"\tpushw\t%ax\n"
#elif __i386__
	"\tmovl\t(%esp), %eax\n"
	"\tleal\t4(%esp), %ebx\n"
	"\tleal\t8(%esp,%eax,4), %ecx\n"
	"\tpushl\t%ecx\n"
	"\tpushl\t%ebx\n"
	"\tpushl\t%eax\n"
#elif __amd64__
	"\tmovq\t(%rsp), %rdi\n"
	"\tleaq\t8(%rsp), %rsi\n"
	"\tleaq\t16(%rsp,%rdi,8), %rdx\n"
#else
# error
#endif
	"\tcall\tmain\n"
#if __ia16__
	"\tpushw\t%ax\n"
#elif __i386__
	"\tpushl\t%eax\n"
#elif __amd64__
	"\tmovq\t%rax, %rdi\n"
#else
# error
#endif
	"\tcall\texit\n"
);

noreturn void exit(int status)
{
# if __ia16__ || __i386__
	asm("int\t$0x80"
		:
		: "a"(1), "b"(status));
# elif __amd64__
	asm("syscall"
		:
		: "a"(60), "D"(status));
# else
#  error
# endif
	for(;;)
		;
}

ssize_t write(int fd, const void * buf, size_t count)
{
	ssize_t result;
# if __ia16__ || __i386__
	asm(
		"int\t$0x80"
			: "=a"(result)
			: "a"(4), "b"(fd), "c"(buf), "d"(count));
# elif __amd64__
	asm("syscall"
			: "=a"(result)
			: "a"(1), "D"(fd), "S"(buf), "d"(count));
# else
#  error
# endif
	return result;
}

ssize_t read(int fd, void * buf, size_t count)
{
	ssize_t result;
# if __ia16__ || __i386__
	asm(
		"int\t$0x80"
			: "=a"(result)
			: "a"(3), "b"(fd), "c"(buf), "d"(count));
# elif __amd64__
	asm("syscall"
			: "=a"(result)
			: "a"(0), "D"(fd), "S"(buf), "d"(count));
# else
#  error
# endif
	return result;
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


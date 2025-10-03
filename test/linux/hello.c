
typedef unsigned long size_t;
typedef long ssize_t;

asm(
	".global\t_start\n"
	"_start:\n"
	"\tcall\tmain\n"
#if __i386__
	"\tpushl\t%eax\n"
#elif __amd64__
	"\tmovq\t%rax, %rdi\n"
#else
# error
#endif
	"\tcall\texit\n"
);

_Noreturn void exit(int status)
{
#if __i386__
	asm("int\t$0x80"
		:
		: "a"(1), "b"(status));
#elif __amd64__
	asm("syscall"
		:
		: "a"(60), "D"(status));
#else
# error
#endif
	for(;;)
		;
}

ssize_t write(int fd, const void * buf, size_t count)
{
	ssize_t result;
#if __i386__
	asm(
		"int\t$0x80"
			: "=a"(result)
			: "a"(4), "b"(fd), "c"(buf), "d"(count));
#elif __amd64__
	asm("syscall"
			: "=a"(result)
			: "a"(1), "D"(fd), "S"(buf), "d"(count));
#else
# error
#endif
	return result;
}

int main()
{
	write(1, "Hello!\n", 7);
	return 123;
}



typedef unsigned size_t;
typedef int ssize_t;

void exit(int status) __smallc;
ssize_t write(int fd, const char * buf, size_t count) __smallc;

int main();

void start()
{
	exit(main());
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

int main()
{
	write(1, "Hello!\n", 7);
	return 123;
}


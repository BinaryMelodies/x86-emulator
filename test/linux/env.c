
#include "system.h"

int main(int argc, char ** argv, char ** envp)
{
	putstr("argc=");
	putint(argc);
	putstr("\n");

	for(int i = 0; i < argc; i++)
	{
		putstr("argv[");
		putint(i);
		putstr("]=\"");
		putstr(argv[i]);
		putstr("\"\n");
	}

	for(int i = 0; envp[i] != 0; i++)
	{
		putstr("envp[");
		putint(i);
		putstr("]=\"");
		putstr(envp[i]);
		putstr("\"\n");
	}

	return 123;
}


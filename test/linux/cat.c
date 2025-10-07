
#include "system.h"

int main(int argc, char * argv[], char * envp)
{
	char c;
	while(read(0, &c, 1) == 1)
	{
		write(1, &c, 1);
	}
	return 0;
}


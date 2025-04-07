
CFLAGS=-Wall -Wextra -g -lm
SOURCES=x86emu.c cpu/cpu.c cpu/x86.gen.c cpu/cpu.h cpu/support.h cpu/general.h cpu/registers.c cpu/protection.c cpu/memory.c cpu/smm.c cpu/x87.c cpu/x86.c cpu/x80.c cpu/x89.c cpu/parse.c

../x86emu: $(SOURCES)
	gcc $(CFLAGS) -o $@ x86emu.c cpu/cpu.c

cpu/x86.gen.c: cpu/generate.py cpu/x86.isa
	python3 cpu/generate.py cpu/x86.isa

clean:
	rm -rf ../x86emu cpu/x86.gen.c cpu/x86.list.c

distclean: clean
	rm -rf *~ cpu/*~


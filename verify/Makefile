
I88_SOURCES := $(shell ls 8088/*)
I88_TESTS = $(I88_SOURCES:.gen.c=)

V20_SOURCES := $(shell ls v20/*)
V20_TESTS = $(V20_SOURCES:.gen.c=)

EMUSOURCES=../src/cpu/cpu.c ../src/cpu/x86.gen.c ../src/cpu/cpu.h ../src/cpu/support.h ../src/cpu/general.h ../src/cpu/registers.c ../src/cpu/protection.c ../src/cpu/memory.c ../src/cpu/smm.c ../src/cpu/x87.c ../src/cpu/x86.c ../src/cpu/x80.c ../src/cpu/x89.c ../src/cpu/parse.c ../src/cpu/x86.gen.c

#all: $(I88_TESTS) $(V20_TESTS)
all: $(V20_TESTS)

../src/cpu/x86.gen.c: ../src/cpu/x86.isa
	make -C ../src cpu/x86.gen.c

8088/%: 8088/%.gen.c verify.c $(EMUSOURCES)
	gcc -o $@ verify.c ../src/cpu/cpu.c -DGENFILE=\"$<\" -lm -DCPU_TYPE=X86_CPU_8086
	strip $@
	#gcc -o $@ verify.c ../src/cpu/cpu.c -DGENFILE=\"$<\" -lm -g

v20/%: v20/%.gen.c verify.c $(EMUSOURCES)
	gcc -o $@ verify.c ../src/cpu/cpu.c -DGENFILE=\"$<\" -lm -DCPU_TYPE=X86_CPU_V20
	strip $@

.PHONY: all


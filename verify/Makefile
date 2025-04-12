
SOURCES := $(shell ls 8088/*)
TESTS = $(SOURCES:.gen.c=)

EMUSOURCES=../src/cpu/cpu.c ../src/cpu/x86.gen.c ../src/cpu/cpu.h ../src/cpu/support.h ../src/cpu/general.h ../src/cpu/registers.c ../src/cpu/protection.c ../src/cpu/memory.c ../src/cpu/smm.c ../src/cpu/x87.c ../src/cpu/x86.c ../src/cpu/x80.c ../src/cpu/x89.c ../src/cpu/parse.c ../src/cpu/x86.gen.c

all: $(TESTS)

../src/cpu/x86.gen.c: ../src/cpu/x86.isa
	make -C ../src cpu/x86.gen.c

%: %.gen.c verify.c $(EMUSOURCES)
	gcc -o $@ verify.c ../src/cpu/cpu.c -DGENFILE=\"$<\" -lm
	strip $@

.PHONY: all


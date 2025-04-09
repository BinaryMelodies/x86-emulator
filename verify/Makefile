
SOURCES := $(shell ls 8088/*)
TESTS = $(SOURCES:.gen.c=)

all: $(TESTS)

%: %.gen.c verify.c
	gcc -o $@ verify.c ../src/cpu/cpu.c -DGENFILE=\"$<\" -lm
	strip $@


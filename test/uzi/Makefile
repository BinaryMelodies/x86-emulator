
all: hello.bin

clean:
	rm -rf *.bin *.elf *.o *.asm

distclean: clean
	rm -rf *~

.PHONY: all optional clean distclean

%.bin: %.c system.h system.o
	sdcc -S -mz80 --code-loc 0x0103 --asm=gas --codeseg .text --constseg .rodata --dataseg .bss $< -o $(patsubst %.bin,%.asm,$@)
	z80-elf-as -sdcc $(patsubst %.bin,%.asm,$@) -o $(patsubst %.bin,%.o,$@)
	z80-elf-ld system.o $(patsubst %.bin,%.o,$@) -o $(patsubst %.bin,%.elf,$@) -T../../tools/uzi.ld -L../../external -lz80
	../../tools/elf2uzi $(patsubst %.bin,%.elf,$@) $@

system.o: system.c
	sdcc -S -mz80 --code-loc 0x0103 --asm=gas --codeseg .text --constseg .rodata --dataseg .bss $< -o $(patsubst %.o,%.asm,$@)
	z80-elf-as -sdcc $(patsubst %.o,%.asm,$@) -o $@

.PHONY: all optional clean distclean


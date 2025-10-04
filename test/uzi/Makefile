
all: hello.80.bin

clean:
	rm -rf *.bin *.elf

distclean: clean
	rm -rf *~

.PHONY: all optional clean distclean

%.80.bin: %.c
	sdcc -S -mz80 --code-loc 0x0103 --asm=gas --codeseg .text --constseg .rodata --dataseg .bss $< -o $(patsubst %.bin,%.asm,$@)
	z80-elf-as -sdcc $(patsubst %.bin,%.asm,$@) -o $(patsubst %.bin,%.o,$@)
	z80-elf-ld $(patsubst %.bin,%.o,$@) -o $(patsubst %.bin,%.elf,$@) -T../../tools/uzi.ld
	../../tools/elf2uzi $(patsubst %.bin,%.elf,$@) $@

.PHONY: all optional clean distclean


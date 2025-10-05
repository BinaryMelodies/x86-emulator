
all: hello.32.elf hello.64.elf env.32.elf env.64.elf
optional: hello.16.bin hello.16.com.bin env.16.bin env.16.com.bin

clean:
	rm -rf *.bin *.elf

distclean: clean
	rm -rf *~

.PHONY: all optional clean distclean

# split I/D
%.16.bin: %.c system.h system.c
	ia16-elf-gcc -o $(patsubst %.bin,%.elf,$@) system.c $< -nostdlib -T../../tools/split.ld -mprotected-mode
	../../tools/elf2elks $(patsubst %.bin,%.elf,$@) $@

# combined I/D
%.16.com.bin: %.c system.h system.c
	ia16-elf-gcc -o $(patsubst %.bin,%.elf,$@) system.c $< -nostdlib -T../../tools/combined.ld -mprotected-mode
	../../tools/elf2elks $(patsubst %.bin,%.elf,$@) $@

%.32.elf: %.c system.h system.c
	i686-elf-gcc -o $@ system.c $< -nostdlib

%.64.elf: %.c system.h system.c
	x86_64-elf-gcc -o $@ system.c $< -nostdlib

.PHONY: all optional clean distclean


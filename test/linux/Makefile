
all: hello.32.bin hello.64.bin

clean:
	rm -rf *.bin *.elf

distclean: clean
	rm -rf *~

%.16.bin: %.c
	ia16-elf-gcc -o $(patsubst %.bin,%.elf,$@) $< -nostdlib -T../../external/split.ld -mprotected-mode
	../../external/elf2elks $(patsubst %.bin,%.elf,$@) $@

%.16.com.bin: %.c
	ia16-elf-gcc -o $(patsubst %.bin,%.elf,$@) $< -nostdlib -T../../external/combined.ld -mprotected-mode
	../../external/elf2elks $(patsubst %.bin,%.elf,$@) $@

%.32.bin: %.c
	i686-elf-gcc -o $@ $< -nostdlib

%.64.bin: %.c
	x86_64-elf-gcc -o $@ $< -nostdlib


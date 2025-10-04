
all: hello.32.elf hello.64.elf

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

%.32.elf: %.c
	i686-elf-gcc -o $@ $< -nostdlib

%.64.elf: %.c
	x86_64-elf-gcc -o $@ $< -nostdlib


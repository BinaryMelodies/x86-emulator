
all: hello.32.bin hello.64.bin

clean:
	rm -rf *.bin

distclean: clean
	rm -rf *~

%.32.bin: %.c
	i686-elf-gcc -o $@ $< -nostdlib

%.64.bin: %.c
	x86_64-elf-gcc -o $@ $< -nostdlib


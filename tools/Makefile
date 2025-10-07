
all: elf2elks elf2uzi

%: %.c
	gcc -o $@ $<

clean:
	rm -rf elf2elks elf2uzi

distclean: clean
	rm -rf *~


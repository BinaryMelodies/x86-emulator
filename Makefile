
all: x86emu tests

x86emu:
	make -C src ../x86emu

tests:
	make -C test

clean:
	make -C src $@
	make -C test $@

distclean: clean
	make -C src $@
	make -C test $@
	rm -rf *~

.PHONY: all clean distclean x86emu tests


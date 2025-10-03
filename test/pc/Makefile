
all: testscr.com testtext.com testkbd.com

clean:
	rm -rf *.com

distclean: clean
	rm -rf *~

testscr.com: testscr.asm
	nasm -fbin $< -o $@

testtext.com: testtext.asm
	nasm -fbin $< -o $@

testkbd.com: testkbd.asm
	nasm -fbin $< -o $@


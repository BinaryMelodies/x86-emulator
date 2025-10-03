
all: 8086.img 286.img 386.img x86-64.img testpcjr.jrc testact.img testpc88.img

clean:
	rm -rf *.img *.jrc

distclean: clean
	rm -rf *~

8086.img: boot.asm
	nasm -fbin $< -o $@ -DOS86=1

286.img: boot.asm
	nasm -fbin $< -o $@ -DOS286=1

386.img: boot.asm
	nasm -fbin $< -o $@ -DOS386=1

x86-64.img: boot.asm
	nasm -fbin $< -o $@ -DOSX64=1

testpcjr.jrc: testpcjr.asm
	nasm -fbin $< -o $@

testact.img: testact.asm
	nasm -fbin $< -o $@

testpc88.img: testpc88.asm
	nasm -fbin $< -o $@

.PHONY: all clean distclean



all: 86.img 286.img 386.img x64.img cpu.com testv33.img testv25.img testv55.img testx87.com testscr.com testtext.com testkbd.com testpcjr.jrc testact.img testpc88.img testrel.bin dos.com
# testi89.bin testv20.img

clean:
	rm -rf *.img *.com *.jrc *.hex *.bin

distclean: clean
	rm -rf *~

86.img: boot.asm
	nasm -fbin $< -o $@ -DOS86=1

286.img: boot.asm
	nasm -fbin $< -o $@ -DOS286=1

386.img: boot.asm
	nasm -fbin $< -o $@ -DOS386=1

x64.img: boot.asm
	nasm -fbin $< -o $@ -DOSX64=1

cpu.com: cpu.asm
	nasm -fbin $< -o $@

testi89.bin: testi89.asm
	asi89 $< -o $(basename $@ .bin).hex
	hex2bin -e bin $(basename $@ .bin).hex

testv20.img: testv20.asm
	nasm -fbin $< -o $@

testv33.img: testv33.asm
	nasm -fbin $< -o $@

testv25.img: testv25.asm
	nasm -fbin $< -o $@

testv55.img: testv55.asm
	nasm -fbin $< -o $@

testx87.com: testx87.asm
	nasm -fbin $< -o $@

testscr.com: testscr.asm
	nasm -fbin $< -o $@

testtext.com: testtext.asm
	nasm -fbin $< -o $@

testkbd.com: testkbd.asm
	nasm -fbin $< -o $@

testpcjr.jrc: testpcjr.asm
	nasm -fbin $< -o $@

testact.img: testact.asm
	nasm -fbin $< -o $@

testpc88.img: testpc88.asm
	nasm -fbin $< -o $@

testrel.bin: testrel.asm
	nasm -fbin $< -o $@

dos.com: dos.asm
	nasm -fbin $< -o $@

.PHONY: all clean distclean



all: cpu.com testv20.img testv33.img testv25.img testv55.img testx87.com testrel.bin
# testi89.bin

clean:
	rm -rf *.img *.com *.hex *.bin

distclean: clean
	rm -rf *~

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

testrel.bin: testrel.asm
	nasm -fbin $< -o $@


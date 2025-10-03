
all: hello.com key.com

clean:
	rm -rf hello.com key.com

distclean: clean
	rm -rf *~

%.com: %.asm
	nasm -fbin $< -o $@


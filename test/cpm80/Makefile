
all: hello.com key.com cat.com

clean:
	rm -rf hello.com key.com cat.com

distclean: clean
	rm -rf *~

%.com: %.asm
	nasm -fbin $< -o $@


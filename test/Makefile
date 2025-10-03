
all:
	make -C boot all
	make -C cpu all
	make -C pc all
	make -C msdos all
	make -C cpm80 all
	make -C linux all

clean:
	make -C boot clean
	make -C cpu clean
	make -C pc clean
	make -C msdos clean
	make -C cpm80 clean
	make -C linux clean

distclean: clean
	rm -rf *~
	make -C boot distclean
	make -C cpu distclean
	make -C pc distclean
	make -C msdos distclean
	make -C cpm80 distclean
	make -C linux distclean

.PHONY: all clean distclean


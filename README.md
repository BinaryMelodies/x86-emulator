
# WARNING!

THIS PROJECT IS EXPERIMENTAL, UNTESTED AND UNFINISHED, USE AT YOUR OWN RISKS

The source code is provided as-is, in the hopes it will be useful for some

# A comprehensive and accurate x86 emulator

This is a project to create an x86 emulation library with a special focus on accuracy and emulating specific old architectures.
It aims to reproduce the behavior and feature set of these CPUs as close as possible.
As part of this goal, many older microprocessors have been researched.
The code specifically includes support for:

* Intel 8086/8088, Intel 80286, Intel 80386, Intel 80486, Intel Pentium
* Differences in Intel64 and AMD64
* Oddities such as Intel 80376 and intermediate steppings of 80386/80486
* NEC processors, including: V20 (with 8080 emulation), µPD9002 (with full Z80 emulation), V33, V25 and V55 (including bank registers)
* Emulating a separate 8087/80287/80387 CPU
* Cyrix CPUs and special registers (preliminary)
* Parsing of all instructions, even those that will not be implemented
* ICE (STOREALL/LOADALL) and SMM emulation (preliminary)
* Intel 8089

What is not the goal of this project (as tempting as it is):

* Fully implementing the floating point and vector instruction sets
* Implementing alternative instruction sets (aside from the 8080/Z80 emulation), such as the VIA alternate instruction set
* Including a full PC emulator (although basic support for various non-IBM compatibles is provided)

# How to use it and run it?

The simplest way to test it out is to run `make x86emu` and then run `./x86emu` with an image file.
The emulator supports running code in a standard MS-DOS .com/.exe file or the first 512 bytes of a floppy disk image.
Note however that none of the MS-DOS API nor PC BIOS is implemented.
The emulator also supports very basic text mode emulation with keyboard interrupts.

The src/cpu folder contains the sources for the CPU emulator.
The file src/emu.c contains the main loop for the PC emulator.
The src/test folder contains a couple of tests

To build the emulator, a standard compliant C compiler (such as the GNU C compiler) is sufficient.
The tests have mostly been written for the Netwide Assembler (NASM).
The file testi89.asm requires an 8089 assembler such as [this one](https://github.com/brouhaha/i89).
The file testv20.asm requires the [i8080.inc macro package](https://github.com/BinaryMelodies/nasm-i8080) to compile Intel 8080 code.

# What works?

Very little of the code has been tested.
The tests included show that some very basic tasks are possible:

* Switching into protected mode and long mode
* NEC specific instructions
    * V20: 8080 emulation
    * V33: extended addressing mode
    * V25: register banks
    * V55: register banks and extended segments
* Having a separate or integrated x87 coprocessor
* Testing for early CPU types (8086/80286, etc.)
* Simple CPUID instructions
* Simplistic text mode and keyboard interrupt emulation for several 8086 based computers
* Several computer types:
    * IBM PC, with MDA, CGA text mode
    * IBM PCjr
    * NEC PC-98
    * NEC PC-88 VA
    * Apricot PC
* Code should compile on big endian targets as well

# What needs testing?

Much of the code has not been tested yet, too much to list here.
In particular, thoroughly testing 8086 mode and protected mode functionality is needed.
The ICE and SMM modes have not been tested yet.

# What needs implementing?

Much of SMM is still missing.
There are many machine specific registers, instructions and behavior (for example, CPUID) that is missing.
Some information is lacking (for example, segment descriptor cache formats, VIA 64-bit implementation details).

# Acknowledgements

This project would not have been possible without the resources and amazing work of many people and it was inspired by many of them, including:

* Intel, AMD and Cyrix manuals
* [Wikipedia](https://www.wikipedia.org/), which contains a [plethora](https://en.wikipedia.org/wiki/X86_instruction_listings) of information on older microprocessors
* [sandpile.org](https://sandpile.org/)
* [The OSDev wiki](https://wiki.osdev.org)
* [OPCODE.LST](http://phg.chat.ru/opcode.txt)
* [https://rep-lodsb.mataroa.blog/blog/intel-286-secrets-ice-mode-and-f1-0f-04/] for documenting how STOREALL works on an Intel 286
* [Robert R. Collins's excellent resources](https://www.rcollins.org/) on Intel CPUs up until the late 90s
* and many more resources I don't remember

Furthermore, this project took inspiration from many other emulator projects, including:

* [QEMU](https://www.qemu.org/) and [Unicorn](https://www.unicorn-engine.org/)
* [Bochs](https://bochs.sourceforge.io/)
* [DOSBox](https://www.dosbox.com/) and [DOSBox-X](https://dosbox-x.com/)
* [PCem](https://www.pcem-emulator.co.uk/) and [86Box](https://86box.net/)
* [libx86emu](https://github.com/wfeldt/libx86emu)
* [Fake86](https://sourceforge.net/projects/fake86/) (to which [I added 8080 emulation support](https://github.com/BinaryMelodies/fake86), similarly to NEC V20)


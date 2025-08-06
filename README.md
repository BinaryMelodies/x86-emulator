
# WARNING!

THIS PROJECT IS EXPERIMENTAL, UNTESTED AND UNFINISHED, USE AT YOUR OWN RISKS

The source code is provided as-is, in the hopes it will be useful for some.
It is principally for educational and research purposes, and it is based on information available online, from manuals and second sources.
See acknowledgements for further details.

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

* Intel 8088 single step test suite passes for documented flags
* Switching into protected mode and long mode
* NEC specific instructions
    * V20: 8080 emulation
    * V33: extended addressing mode
    * V25: register banks
    * V55: register banks and extended segments
* Having a separate or integrated x87 coprocessor
* Testing for early CPU types (8086/80286, etc.)
* Simplistic instruction cache, sufficient to distinguish between an 8086 and an 8088
* Simple CPUID instructions
* Simplistic text mode and keyboard interrupt emulation for several 8086 based computers
* Several computer types:
    * IBM PC, with MDA, CGA text mode
    * IBM PCjr
    * NEC PC-98
    * NEC PC-88 VA
    * Apricot PC
* Code should compile on big endian targets as well

## Notes on NEC V30 and µPD9002 emulation

These CPUs support an 8-bit emulation mode when bit 15 (MD) of the FLAGS register (referred to as PSW in NEC documents) is cleared.
The current implementation fully follows the Intel 8080 (for V30) and Zilog Z80 (for µPD9002) execution, except for two additional instructions: RETEM and CALLN.
In particular, all flag affection behaves according to the original chips.
Whether this is the exact behavior of the NEC CPUs in emulation mode is not currently known.

Very little information is available on the NEC µPD9002.
The current implementation attempts to combine a full V30 and Z80 component.
This means:

* A, F, BC, DE, HL, SP, PC are mapped to AL, low byte of FLAGS (PSW), CX (CW), DX (DW), BX (BW), BP, IP (PC), as for the V30.
The V30 SP register is reserved for handling (V30 mode) interrupts.
* The IFF1 flag maps to the IF (IE) flag.
* IX and IY are mapped to SI (IX) and DI (IY) as has been confirmed by other users.
* A second set of AF', BC', DE', HL' registers are provided, only the currently selected ones are visible in V30 mode. (This means that it is possible to hide the current values of the AW/BW/CW/DW registers by switching to Z80 mode, issuing the appropriate instructions and returning to V30 mode.)
* The FLAGS register is extended with Z80 specific flags, but these are only available in Z80 mode, as well as the FLAGS register image pushed onto the stack during an interrupt.
In V30 mode, bits 1, 3, 5 are still 1, 0, 0, as for V30.
Only an IRET (RETI) instruction can restore them outside Z80 emulation.
* The IFF2, I, R and IM are supported, as well as interrupt modes 0, 1 and 2.
* As per NEC documentation, I/O instructions, LD A, R invoke an interrupt handler.
The exception is when the CPU is in full Z80 emulation (BRKEM2).
It is unknown whether this is the actual behavior of a µPD9002 in this mode.

# What needs testing?

Much of the code has not been tested yet, too much to list here.
In particular, thoroughly testing 8086 mode and protected mode functionality is needed.
The ICE and SMM modes have not been tested yet.

# What needs implementing?

Much of SMM is still missing.
There are many machine specific registers, instructions and behavior (for example, CPUID) that is missing.
Some information is lacking (for example, segment descriptor cache formats, VIA 64-bit implementation details).
Very little is known about the NEC µPD9002, the emulator attempts to simulate a full Z80 instruction set.
Some preliminary code is available for handling the Intel 8086/8088 internal registers, but most of it is not yet implemented.

# Acknowledgements

This project would not have been possible without the resources and amazing work of many people and it was inspired by many of them, including:

* Intel, AMD and Cyrix manuals
* [Wikipedia](https://www.wikipedia.org/), which contains a [plethora](https://en.wikipedia.org/wiki/X86_instruction_listings) of information on older microprocessors
* [sandpile.org](https://sandpile.org/)
* [The OSDev wiki](https://wiki.osdev.org)
* [OPCODE.LST](http://phg.chat.ru/opcode.txt)
* [rep-lodsb.mataroa.blog](https://rep-lodsb.mataroa.blog/blog/intel-286-secrets-ice-mode-and-f1-0f-04/) for documenting how STOREALL works on an Intel 286
* [Robert R. Collins's excellent resources](https://www.rcollins.org/) on Intel CPUs up until the late 90s
* [x86, x64 Instruction Latency, Memory Latency and CPUID](http://users.atw.hu/instlatx64/)
* Single step tests for the [Intel 8086](https://github.com/SingleStepTests/8088) and the [NEC V20](https://github.com/SingleStepTests/v20)
* and many more resources I don't remember

Furthermore, this project took inspiration from many other emulator projects, including:

* [QEMU](https://www.qemu.org/) and [Unicorn](https://www.unicorn-engine.org/)
* [Bochs](https://bochs.sourceforge.io/)
* [DOSBox](https://www.dosbox.com/) and [DOSBox-X](https://dosbox-x.com/)
* [PCem](https://www.pcem-emulator.co.uk/) and [86Box](https://86box.net/)
* [libx86emu](https://github.com/wfeldt/libx86emu)
* [Fake86](https://sourceforge.net/projects/fake86/) (to which [I added 8080 emulation support](https://github.com/BinaryMelodies/fake86), similarly to NEC V20)
* [MartyPC](https://github.com/dbalsom/martypc)


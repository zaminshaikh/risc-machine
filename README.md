# risc-machine
Virtual machine in C that converts binary object files to MIPS ISA, updating the registers and memory addresses after executing each instruction

The Simplified RISC Machine (SRM) processor’s instruction set architecture (ISA) is simplified from the
MIPS processor’s ISA [2]. In particular, SRM is a little-endian machine with 32-bit (4-byte) words. All
instructions are also 32-bits wide and there is no floating-point support, kernel mode support, or other
advanced features.

The main module of the machine is accessed through 'machine.c' and 'machine.h'

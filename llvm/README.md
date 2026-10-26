# llvm

LLVM backend for the CPU, kept here as an out-of-tree target directory and patches.

The llvm-project checkout and its build directories are git-ignored.

Planned order:
1. Target description and assembly output, assembled with customasm
2. Clang target info (`int` = 16 bits)
3. MC layer with object-file output, plus lld relocations
4. picolibc

References: the Cpu0 tutorial for overall structure, the MSP430 backend for 16-bit details, the AVR backend for register pairs, and the RISC-V backend for `CompressPat`.

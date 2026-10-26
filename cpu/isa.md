# fpga_cpu architecture reference

Status: draft, version 0.1. The authoritative definitions are in
[`isa.yaml`](isa.yaml). This page explains them.

## Registers

- 16 general registers `r0`–`r15`, each 16 bits.
- The even/odd pairs form 8 pointer registers `p0`–`p7`. `pN` is
  `r(2N+1):r(2N)`, with the low 16 bits in the even register.
- **Pointer use:** when a pair is used as an address, only bits 23:0
  count; the top byte of the odd register is ignored. Instructions that
  write a pair (`LEA`, `ADDP*`, `[pN]+`, `-[pN]`, `JAL`) produce the full
  32-bit result, so `ADDPP` is a complete 32-bit add and `ADDPI`/`LEA` are
  32-bit add-immediate. The compiler uses them for `long` arithmetic as
  well as for pointers.
- **Raw 32-bit use:** `LDP`, `STP` and `MOVP` move all 32 bits, so a pair
  can also hold a C `long`.
- The PC is 24 bits. Instructions are 2-byte aligned.

| Pair | Registers | Conventional use |
|---|---|---|
| p0 | r1:r0 | scratch; the assembler's temporary for direct addressing in variant A |
| p1–p2 | r3:r2, r5:r4 | arguments and result (r2, or p1 for 32-bit/pointer values) |
| p3 | r7:r6 | temporaries |
| p4–p5 | r9:r8, r11:r10 | callee-saved |
| p6 = `lr` | r13:r12 | return address (written by `JAL`/`JALR`) |
| p7 = `sp` | r15:r14 | stack pointer; the stack grows down |

## Flags

N, Z, C, V.

| Instructions | Flags |
|---|---|
| Arithmetic: `ADD ADC SUB SBC CMP CPC NEG ADDI CMPI` | Set NZCV. For subtraction, C=1 means no borrow (ARM convention). |
| `SBC`, `CPC` | Additionally chain Z: Z ends up set only if it was already set and this result is zero. That makes 32-bit compares `CMP lo; CPC hi` and branches on EQ/NE/LO/HS correct. |
| Logic: `AND OR XOR NOT ANDI ORI XORI` | Set NZ, clear V, leave C alone |
| Shifts | Set NZ. C = the last bit shifted out (unchanged if the shift count is 0). |
| Moves, multiplies, loads/stores, pointer arithmetic | Leave flags alone |

Conditions: `EQ NE CS CC MI PL VS VC HI LS GE LT GT LE AL`. `HS` = CS, `LO` =
CC, `RA` = AL.

## Addressing modes

| Mode | Syntax | Effective address |
|---|---|---|
| d | `[pN + disp]` | pN + sign-extended displacement |
| x | `[pN + rX*s]`, `[pN + rX.z*s]` | pN + (sign- or zero-extended rX) × s, where s = 1, 2 or 4 |
| pi | `[pN]+` | pN, then pN += access size |
| pd | `-[pN]` | pN -= access size, then pN |
| a | `[abs24]` | absolute address. Variant B has direct forms. Variant A uses `LEA` + d mode. |

16-bit words and 32-bit pairs must be 2-byte aligned. A pair at an address
that is 2 mod 4 costs an extra cycle, because it spans two SRAM words.

## Instruction lengths

- **Length:** an instruction is one 16-bit parcel or two. The first
  parcel's top bits decide:
  - Variant A: `1101` or `111x` means 32-bit. 3/16 of the opcode space.
  - Variant B: MSB `0` means 32-bit. Half of the opcode space.
- **32-bit layout:** the first parcel sits at the lower address and holds
  the instruction's high half.
- **Fetch:** the I-cache delivers a 64-bit window, so every instruction is
  one fetch.
- **Assembler:** it always picks the shortest encoding that fits. 16-bit
  forms are the common special cases: two-operand ALU, small immediates,
  short displacements, SP-relative access, short branches.

## Memory map

| Range | Use |
|---|---|
| 000000–0FFFFF | 1 MB SRAM, 32-bit, holding code and data |
| 100000–EFFFFF | reserved |
| F00000–F0FFFF | I/O (simulator registers: see `sim/README.md`) |
| FF0000–FFFFFF | boot ROM (block RAM) |

## Not yet defined

- Interrupts and exceptions. `RETI` traps in the simulator.
- System registers beyond SR0 = flags, SR1/SR2 = cycle counter, SR3 = instructions retired.
- MMU/banking.
- Division instructions. Division is in software for now.

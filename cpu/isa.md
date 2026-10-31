# fpga_cpu architecture reference

Status: draft, version 0.1. The authoritative definitions are in
[`isa.yaml`](isa.yaml). This page explains them.

## Registers

- 16 general registers `r0`–`r15`, each 16 bits.
- The even/odd pairs form 8 pointer registers `p0`–`p7`. `pN` is
  `r(2N+1):r(2N)`, with the low 16 bits in the even register.
- **Pair fields:** instructions that name a pair in a 4-bit register field
  (`ADDPI`, `ADDP*`, `MULPU`, `FSHR`/`FSHL`, `DSTEP`, and `LDP`/`STP`/`LEA`
  in the 32-bit memory forms) hold the even register number. An odd number
  there is a reserved encoding and traps as an illegal instruction; it
  does not alias the even pair.
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
| Shifts | Set NZ, clear V. C = the last bit shifted out (unchanged if the shift count is 0). |
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

**Store data and write-back:** a store's data (`rD` or the pair `pD`) is the
register value before the instruction's own pointer write-back. `stw r4,[p2]+`
with r4 inside p2 stores the old r4, and `stp p3,-[p3]` stores the old p3.
(For loads the loaded value wins over the write-back.)

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

## Extension X (optional)

Four arithmetic helpers, all 32-bit encodings in the `REG3` group, for the
runtime library's multiply, divide, shift and soft-float routines. They
are candidates, not committed hardware: the simulator runs them only with
`fsim --ext` (otherwise they trap as illegal), and `fcc --ext[=parts]`
builds a runtime that uses them. `benchmarks/run.py` measures each one
(configurations `x-dstep`, `x-mulpu`, `x-fsh`, `x-clz`, `ext`).

| Instruction | Operation | Flags | Used by |
|---|---|---|---|
| `DSTEP pD, pS` | one restoring-division step: t = pD*2 + C (33 bits); if t >= pS then pD = t - pS, C = 1, else pD = t, C = 0 | C | 32- and 64-bit divide, 16-bit divide, float divide |
| `MULPU pD, rS, rT` | pD = rS * rT, the full unsigned 32-bit product | none | 32- and 64-bit multiply, float multiply |
| `FSHR rD, pS, rT` | rD = low 16 bits of pS >> (rT & 15) (funnel shift) | none | variable 32-bit shifts, float add alignment |
| `FSHL rD, pS, rT` | rD = high 16 bits of pS << (rT & 15) | none | variable 32-bit shifts |
| `CLZ rD, rS` | rD = number of leading zero bits of rS (16 for 0) | none | int-to-float conversion |

`DSTEP` is used with the dividend shifted through C: `ADD`/`ADC` move the
next dividend bit into C, `DSTEP` consumes it and leaves the quotient bit
in C, and the next `ADC` pair shifts that bit in while moving the next
dividend bit out. A 32-bit quotient bit costs three instructions
(`DSTEP`, `ADC`, `ADC`) instead of about thirteen.

Rough extra cost on the XC3S200 (LUT4), design estimates rather than
synthesis results:

| Instruction | Extra LUTs (est.) | What the LUTs buy |
|---|---|---|
| `MULPU` | 15–30 | the multiplier already forms all 32 product bits; a path for the high half to the pair write port |
| `CLZ` | 25–40 | a 16-bit priority encoder and its result mux |
| `FSHR`/`FSHL` | 40–90 | widen the existing 16-bit barrel shifter to take its fill bits from the other half of a pair, and a pair operand into the shifter |
| `DSTEP` | 70–120 | a 33-bit subtract and select on two pair operands (the pointer adder may be shareable), and the C update |

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
- Division instructions. Division is in software (extension X's `DSTEP`
  is a candidate step instruction).

# FC: the pipelined core for FPGAs

`verilog/rtl/fc_core.v` is the FPGA core of the ladder in
[cores.md](cores.md): today's ISA (variant A), one instruction a clock,
built from what an FPGA has. It is the c0 rung; c1 and up add to it.
T1 (docs/t1.md) stays the small byte-serial model and T1F (docs/t1f.md) its
FPGA-tuned form; FC is the core to use where the FPGA has room.

## Why this design

On an FPGA, width is cheap and control is expensive; in transistors it is
the other way round. A 16-bit adder is a carry chain (~16 LUTs), a 16 x 16
multiply is one hard block, the 16 registers are a few LUTs of distributed
RAM, and caches are block RAM. What costs LUTs is sequencing: T1's control
for byte passes is most of its 2,000+ LUTs. A pipeline that does each
instruction in one pass has almost no sequencing, and cores.md's estimates
give it an order of magnitude more MIPS per LUT than any byte-serial core.

## Pipeline

| Stage | Does |
|---|---|
| F | a queue of 16-bit parcels, filled from the instruction port (32-bit words, requests pipelined) |
| D | decode (`t1_dec.v`, shared with T1F) and the register read: three pair-wide ports, with what W writes bypassed |
| X | 16-bit ALU, funnel shifter, 17 x 17 multiply, 32-bit pointer and address adder, branch, jump, faults; the data port request |
| M | the data port's response |
| W | register writes, one per bank (even and odd registers) |

- Registers: two banks of 8 x 16 in distributed RAM, so a pair (the even
  and odd register) reads and writes in one access.
- Forwarding from M and W into X; a load's result one clock late (one
  bubble), as fsim's c0.
- A taken branch or jump redirects fetch from X: two clocks, as c0.
- A load through `[pN]+` or `-[pN]` writes two pairs' worth of halves and
  takes a second clock in W.
- A pair access at 2 mod 4 is two words on the data port.
- A store into code already fetched refetches from the next instruction,
  so self-modifying code needs no SYNC (fsim's semantics).
- Faults are detected in X, in order; a fault or trap halts the core once
  the instructions before it have retired.

Memory ports are synchronous with a grant and in-order responses, so block
RAM, an I-cache, SRAM or SDRAM controllers can sit behind them with any
latency.

## Status

Verified: identical to fsim instruction by instruction on the T1 suites
(`T1_CORE=fc verilog/tb/t1test.py`: 61/61; `t1fuzz.py`: 300 programs, with
and without extension X), also with the ports refusing 30% of requests
(`T1_STALL=30`).

First synthesis (Gowin GW2A, the core alone): ~1,670 LUT4 before place and
route (decoder 205), 4.7 K LUT4 placed (a Gowin flip-flop not fed by its
own LUT costs a pass-through LUT, and this version carries ~670 flip-flops
of pipeline state), 1 multiplier, 24 RAM16SDP4; 36 MHz. Not yet
efficient. Next:

- carry only what each stage needs (the X stage holds the whole decode and
  three 32-bit operands; M and W two 32-bit values each);
- a 4-parcel aligned fetch buffer instead of the 6-parcel shifting queue;
- the store-into-code redirect from M (it is on the longest path now);
- branch targets computed in D;
- then synthesize on the Spartan-3 (ISE) and put it in both boards'
  systems, with an I-cache in front of the Spartan-3's SRAM.

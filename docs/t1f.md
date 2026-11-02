# T1F: T1 for FPGAs

`verilog/rtl/t1f_core.v` is T1 (docs/t1.md) changed where an FPGA wants it.
It runs the same ISA with the same results instruction by instruction; T1
stays the transistor-count model. The changes:

- **Registered decode.** T1 decodes the instruction parcels combinationally
  every clock, and its decode sat at the start of its longest paths (and of
  its bus outputs). T1F registers the decode. Two decoders run every clock:
  one from IR and P2, one from the prefetch buffer NB. An instruction that
  is already whole in NB when the one before ends starts executing at once,
  from NB's decoder (`use_b` for that first clock). One that arrives from
  the bus spends a clock in the new state `S_DEC` while IR's decoder loads.
  For a 32-bit instruction that clock also fetches the second parcel's low
  byte, so it costs nothing extra.
- **din reaches registers only.** Read data goes straight to the register
  file's write data, NB, IR, P2 and the load sign, never through the ALU or
  into the next-state logic (whether a parcel starts a 32-bit instruction
  is decided in `S_DEC`, from IR).
- **Hardware multiply.** MUL, MULH, MULHU and MULPU read the operands' low
  bytes, then their high bytes into one 17 x 17 multiply (a DSP block), then
  write the product's bytes: 4 clocks (6 for MULPU) instead of up to about
  70.
- **SR1-SR3 registered** at the clock edge: MFSR reads the counters as they
  were a clock earlier, and `sr_in` has a whole cycle.
- **Clock enable** (`ce`): low holds every register, for a design that
  stalls the core instead of gating its clock.

Costs: decoding a second instruction (the NB decoder) and the decode
registers, about 200 LUTs and 240 flip-flops more than T1, and one
MULT18X18.

## Timing (Gowin GW2A-18, nextpnr)

The core on its own, with registered inputs and outputs:

| | T1 | T1F |
|---|---|---|
| internal (fmax) | 24.7 ns (40 MHz) | 20.5 ns (49 MHz) |
| clock edge to bus outputs (addr, rd, wr, dout) | 15.7 ns | 11.6 ns |
| din to registers | 12.2 ns | 4.4 ns |

In the Tang system (tang/rtl/system.v) a T1F cycle is two 67.5 MHz clocks
(33.75 MHz) for block RAM: the bus is taken one clock after T1F's edge and
read data has the next. I/O registers and the SDRAM get one clock more. T1
needs four.

On the Spartan-3 (XC3S200-4, ISE 14.7, in fpga/rtl/system.v):

| | T1 | T1F |
|---|---|---|
| internal | 47 ns | 46.4 ns |
| clock edge to bus outputs | 39.9 ns | 37.9 ns |
| SRAM pins to its registers | ~25 ns | 12.1 ns |
| LUTs (whole system) | 3,298 | 3,725 of 3,840 |

There routing is most of every delay, so the bus still needs two 50 MHz
cycles; read data now fits in the other two, so T1F can run at four cycles
a clock (12.5 MHz) against T1's five (10 MHz). It does (`make S3PER=4`),
and the demo runs 1.35x faster, but its palette animation is visibly less
even than at five cycles. On the board the frame loop still runs once a
frame and the palette update ends at the same scan line as in simulation;
filmed, the picture changes less evenly from frame to frame, with no frame
dropped or wrong. The cause is not found yet (the SRAM's real timing
against the 15 ns the constraints assume is a suspect), so the Spartan-3
runs T1F at five cycles for now. The prefetch decoder sees 16-bit
instructions only (its second parcel input is tied to zero), which is what
made the system fit.

## Cycles and speed

`verilog/tb/t1bench.py` (T1F clocks):

| program | T1 cycles/inst | T1F cycles/inst |
|---|---|---|
| arith32 | 11.78 | 4.14 |
| fib | 5.37 | 6.26 |
| lines | 3.01 | 3.81 |
| memops | 3.61 | 4.45 |
| sieve | 3.49 | 4.44 |
| tokenize | 3.29 | 4.06 |

The decode clock adds about 20% to everything but multiplication. The demo:

| board | T1 | T1F |
|---|---|---|
| Tang Nano 20K (clock doubles) | 3.32 MIPS, 20% busy | 6.47 MIPS, 10% busy |
| Spartan-3, both at 10 MHz | 1.81 MIPS, CPI 5.51 | 1.95 MIPS, CPI 5.11 |
| Spartan-3, T1F at 12.5 MHz (S3PER=4) | | 2.45 MIPS, CPI 5.08 |

On the Spartan-3 the demo's CPI drops despite the decode clock: its star
projection multiplies.

## Tests

The T1 suites run on T1F with `T1_CORE=t1f`: `verilog/tb/t1test.py` (61
runs), `t1fuzz.py` (with and without extension X), `t1bench.py`, all in
lockstep with fsim.

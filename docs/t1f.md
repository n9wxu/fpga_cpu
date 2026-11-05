# T1F: T1 for FPGAs

`verilog/rtl/t1f_core.v` is T1 (docs/t1.md) changed where an FPGA wants it.
It runs the same ISA with the same results instruction by instruction; T1
stays the transistor-count model. The changes:

- **Registered decode.** T1 decodes the instruction parcels combinationally
  every clock, and its decode sat at the start of its longest paths (and of
  its bus outputs). T1F registers the decode. Two decoders run every clock:
  one from IR and P2, one from `np`, the parcel IR loads when an instruction
  starts (NB, or NB's low byte and the byte arriving on din). The new
  instruction's first clock uses `np`'s decode registers (`use_b`), the
  rest IR's, so it executes from the clock after its last byte arrives, as
  on T1. When that byte came from din the next state cannot look at it, so
  the first clock is `S_DEC`: a 16-bit instruction executes there (as
  `S_EX`), a 32-bit one fetches its second parcel's low byte instead (where
  T1 would be in `S_P2`). The second parcel's high byte also goes through
  the IR decoder from din.
- **din reaches registers only.** Read data goes straight to the register
  file's write data, NB, IR, P2, the load sign and the decode registers,
  never through the ALU or into the next-state logic.
- **Hardware multiply.** MUL, MULH, MULHU and MULPU read the operands' low
  bytes, then their high bytes into one 17 x 17 multiply (a DSP block), then
  write the product's bytes: 4 clocks (6 for MULPU) instead of up to about
  70.
- **SR1-SR3 registered** at the clock edge: MFSR reads the counters as they
  were a clock earlier, and `sr_in` has a whole cycle.
- **Clock enable** (`ce`): low holds every register, for a design that
  stalls the core instead of gating its clock.
- **24-bit addresses** (parameter `AW24`, on in the Tang system). T1 folds
  its PC and data address into 21 bits (docs/t1.md, "Addresses"): RAM to
  0x0FFFFF, I/O and ROM from 0xF00000, and an address between them halts
  with FD. With `AW24 = 1` PC and AR are 24 bits and the pins carry all of
  them: no fold, no range check (the system decides what is at every
  address), and fetch and data steps wrap from 0xFFFFFF to 0, as fsim's
  addresses do. Only an odd address for a word or pair, or an odd
  `reset_pc`, halts with FD. On the Tang Nano 20K this gives T1F the whole
  8 MB SDRAM (0x000000-0x7FFFFF; it repeats every 8 MB up to the I/O at
  0xF00000). The cycles are the same: the top address byte was always a
  pass of its own. `AW24 = 0`, the default, is the folded core, for the
  Spartan-3, whose 1 MB SRAM and full XC3S200 need no more.
- **Interrupts** (parameter `IRQ`, on in the Tang system), as on T1S
  (docs/t1.md, "Interrupts"): an `irq` input, EPC and IE. At an
  instruction's end (its `done` or `jump`), with IE set and the line high,
  the next instruction does not start: EPC takes its address (past a jump
  the target, else the PC less the bytes already prefetched), IE clears and
  fetching restarts at 0x000010. `RETI` jumps to EPC and sets IE; `MTSR SR6`
  sets IE. With `IRQ = 0` the line is ignored and `RETI` halts (FF), as
  before. On the Tang the line comes from iosys's controller (F00700): the
  vertical blank (source 0) and the audio engine's voice flags (source 3);
  `fpga/lib/irq.c` installs handlers.

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
| arith32 | 11.78 | 3.28 |
| fib | 5.37 | 5.37 |
| lines | 3.01 | 3.01 |
| memops | 3.61 | 3.61 |
| sieve | 3.49 | 3.49 |
| tokenize | 3.29 | 3.25 |

T1F takes T1's cycles or fewer (far fewer where it multiplies). Before the decoders took `np`
and P2's high byte from din, an instruction whose last byte arrived at the
clock it started spent a clock in `S_DEC` first (about 20% more cycles
than T1, sieve 4.44). The demo, measured with that older T1F:

| board | T1 | T1F |
|---|---|---|
| Tang Nano 20K (clock doubles) | 3.32 MIPS, 20% busy | 6.47 MIPS, 10% busy |
| Spartan-3, both at 10 MHz | 1.81 MIPS, CPI 5.51 | 1.95 MIPS, CPI 5.11 |
| Spartan-3, T1F at 12.5 MHz (S3PER=4) | | 2.45 MIPS, CPI 5.08 |

On the Spartan-3 the demo's CPI dropped even with the decode clock: its
star projection multiplies. Without the decode clock the Spartan-3 RAM
machine's T1S (fpga/rtl/ramsys.v, simulated) takes 3.34 steps an
instruction in the demo, against 4.14 before.

## T1S: synchronous and pipelined

`verilog/rtl/t1s_core.v` is T1F for a block-RAM FPGA without asynchronous
RAM to spare, as on the Spartan-3 RAM machine (`fpga/rtl/ramsys.v`). It
runs on the system clock with a clock enable, one step per enabled edge:

- **Decode in a ROM.** The instruction's 10-bit key (`t1s_dec.v`) addresses
  a 1K x 81 block-RAM ROM (`t1s_rom_x*.vh`, made by `verilog/tb/t1sgen.py`
  from T1F's decoder, checked against it on every first parcel); the ROM's
  output register is the decode register.
- **Register file in a block RAM**, read every clock: a step needs at least
  two clocks.
- **Two instructions in flight.** `t1s_fetch` runs at every clock, reading
  32-bit words from the PC on through its own port into a two-word landing
  buffer, which feeds a 4-parcel queue in the core. The next instruction is
  decoded from the queue (or straight from the landing buffer) at the edge
  the current one ends and starts executing at once, so fetching costs no
  steps. The byte bus carries only loads and stores. PC is a register of
  its own; a jump empties the queue and fetching restarts at the target. A
  store within 32 bytes after the PC (everything fetched ahead lies there)
  empties the queue too and the code is fetched again, so the next
  instruction sees the store, as in fsim, with no SYNC.

Steps per instruction (`T1_CORE=t1s T1_PER=4 T1_ILAT=5`: a step every 4
clocks, a fetched word back 5 clocks after it is asked for):

| program | T1 | T1F | T1S |
|---|---|---|---|
| arith32 | 11.78 | 3.28 | 2.71 |
| fib | 5.37 | 5.37 | 4.53 |
| lines | 3.01 | 3.01 | 2.83 |
| memops | 3.61 | 3.61 | 3.38 |
| sieve | 3.49 | 3.49 | 2.67 |
| tokenize | 3.29 | 3.25 | 2.95 |

On the RAM machine a step without a data access takes 4 clocks of 50 MHz
(`cpumem.v`, PERF); one with a load or store 5 or more. The demo runs
1.59x as many instructions in the same time as T1S did before the fetch
unit, and the system uses 3,747 of the XC3S200's 3,840 LUTs.

`T1_ILAT` and `T1_IRAND` (the instruction port takes requests at random)
vary the fetch timing in the test suites.

## Tests

The T1 suites run on T1F with `T1_CORE=t1f`: `verilog/tb/t1test.py` (61
runs), `t1fuzz.py` (with and without extension X), `t1bench.py`, all in
lockstep with fsim. `T1_AW24=1` builds the 24-bit core; the directed tests
then also run `tests/xram.s` (`; xram: 7`: the testbench and fsim `--xram 7`
have 8 MB of RAM), which uses data across 0x100000 and at the top of the
8 MB and runs code from 0x600000 and over 0x100000. `tests/irq.s` runs on
T1F too (the testbench's controller has the software source). `T1_XRAM=N` gives every
run N MB of RAM above the first. On the board, `tang/progs/xramtest.c` tests
every word of 0x100000-0x7FFFFF.

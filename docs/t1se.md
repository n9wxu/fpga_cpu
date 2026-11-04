# T1SE: evaluation (DRAFT)

Draft design study, not an implementation. Every number is marked
**measured** (run or synthesized here, scratch files under the session
scratchpad `t1se/`) or **est.** (a model or an extrapolation).

## What T1S is, and what "1 cycle per instruction" can mean

T1S (`verilog/rtl/t1s_core.v`) is T1F's sequencer with the two things an
FPGA does best asynchronously made synchronous: the 36-byte register file
is one block RAM (`t1s_rf`, read at the clock edge before the step that
uses it) and the decode is a 1K x 81 ROM in block RAM behind a 10-bit key
(`t1s_dec.v`, `t1s_rom_x*.vh`). A step therefore needs two clocks (`ce`
at most every other clock). It is still byte-serial: 8-bit ALU, 8-bit
bus, the same steps as T1F.

Measured, 4 programs on RTL (Icarus, first 2M steps): T1S takes exactly
T1F's step count (dhrystone 4.865, sieve 4.56, bubblesort 4.792, intmm
4.923 steps/instruction). So T1S as written is about 9.7 clocks per
instruction, not 1.

Two readings of "T1SE keeps 1 cycle per instruction":

1. **Step = cycle.** T1SE keeps T1S's one-step-per-cycle timing (one
   `ce` period) and widens what a step does. CPI in cycles falls with the
   steps per instruction.
2. **One instruction per clock** for the common instructions, as FC
   ("one instruction a clock").

"Keeps 1 cycle per instruction" and "a fast version" read as reading 2:
the user wants a T1S-style synchronous machine at CPI near 1. The
evaluation covers both; reading 1 is variant A below.

## Candidates

CPI: instruction-weighted geometric mean over the 14 C programs
(dhrystone, sieve, richards, coremark, 10 Stanford) from `fsim --core c0`
counts; B and C are my model (`t1se/model.py`: fetch parcels, data
accesses, 2-clock taken branch, +1 for pair ALU ops and multiplies).
BASIC: the benchmark images are one interpreter image, so they are not
independent; cores.md's c0 BASIC CPI is 1.72.

| Candidate | Datapath / bus | LUT4 (Gowin placed) | BRAM | CPI | MIPS at 25 MHz | Basis |
|---|---|---|---|---|---|---|
| T1F | 8 / 8 | 2,470 | 0 | 4.87 | 5.1 | measured |
| T1S | 8 / 8, 2 clk/step | 2,000 | 7 (6 on Xilinx) | 4.87 steps = 9.7 clk | 2.6 (5.1 at 50 MHz) | measured |
| A: T1SE step = clock | 8 / 8 | ~2,200 | 7 | 4.87 | 5.1 | LUT est. (lookahead of the next step's register address, ~+200) |
| B: 16-bit | 16 / 16 shared bus, queue | ~2,800 | 2-4 | 1.96 | 12.8 | est. |
| C: 16-bit, 32-bit bus | 16 / 32, 4-parcel queue | ~3,300 | 2-4 | 1.37 (1.4-1.6 allowing for RTL) | 16-18 | est. |
| FC (c0) | 16-32 / 32, two ports | 4,285 (2,825 without pass-through LUTs) | 0 | 1.53 RTL, 1.29 fsim c0, 1.08 fsim c1 | 16.3 | measured (RTL on 8 programs; dhrystone excluded, see below) |

Measured synthesis (`yosys synth_gowin`, then `nextpnr-himbaechel` for
GW2AR-18, core alone behind registers): T1S 2,024 LUT4, 7 BSRAM, 46 MHz;
T1F 2,488 LUT4, 48 MHz; FC 4,285 LUT4, 39 MHz. Those include about 20
LUTs of test wrapper and every flip-flop not fed by its own LUT (Gowin
pass-through LUT1: T1S 534, T1F 425, FC 941). Without them: T1S 1,510,
T1F 1,970, FC 2,825. `synth_xilinx -family xc3s` (experimental, multiplier
removed): T1S 1,831 LUTs, 227 FF, 5 RAMB16_S18 + 1 RAMB16_S9_S9; T1F
2,587; FC 5,743, which is not usable because the register file was not
mapped to distributed RAM there. No ISE run was made, so S3 LUT counts are
est. only.

Notes:

- The decode ROM is 1,024 x 81 but only 520 keys are used and only 183
  distinct words: a key-to-index ROM plus a 256-word table would be about
  3 block RAMs instead of 5 (est.).
- cores.md estimates c0 at 1.0-1.2k LUTs; the measured FC is 2.8-4.3k. The
  estimate is stale until FC is slimmed (docs/fc.md "Next").
- FC on dhrystone in the RTL testbench retired 15,549 instructions in 2M
  cycles (CPI 129) while the other eight programs ran at 1.3-1.7; not
  investigated.

## Where T1SE converges

A one-instruction-a-clock T1SE needs, per clock, two register reads and a
write (three reads for pair addressing), the next instruction's decode,
fetch and a data access at once, and forwarding. That is FC's pipeline.
Three things make a T1S-style machine worse than FC there:

- A block-RAM register file reads synchronously and gives two ports per
  block RAM. Three read ports need 2-3 block RAMs, while FC's 16 registers
  in distributed RAM cost 24 RAM16SDP4 (about 100 LUTs). On the XC3S200,
  with 11 of 12 block RAMs planned, the BRAM register file does not fit.
- The decode ROM in block RAM is a pipeline stage, which FC already has
  (decoder 205 LUTs).
- The per-step control that makes T1S's sequencer large does not exist in
  a pipeline.

What T1S keeps that FC lacks: the fewest logic LUTs (about 1.5k) and a
simple, verified byte-serial sequencer, at the price of block RAMs that
the Spartan-3 board has no spare of. It is not the transistor-count model
(T1 is): the block-RAM structure has no transistor equivalent.

## Memory fit (RAM machine: 32-bit words, 50 MHz, one word a clock)

- 8-bit core (T1S, A): one byte a step, 25 MHz steps: it uses a quarter of
  a word every 2 clocks, so a one-word buffer per port is enough and
  bandwidth is irrelevant. Latency matters: the bus address is
  combinational from the state, and the rammux grant adds clocks, so the
  core stays at 2 clocks a step in practice.
- B: 16-bit shared bus, about 1.2 parcels and 0.35 data accesses per
  instruction, so one word of the RAM machine feeds two parcels; needs a
  fetch buffer that reads ahead, but no I-cache.
- C and FC: 2.3 bytes of fetch and about 0.35 data words an instruction at
  16-18 MIPS is about 65 MB/s of the 200 MB/s the SRAM gives (video takes
  about 7%). At 50 MHz core clock it would be about 135 MB/s and the
  I-cache or a line buffer becomes necessary (cores.md c0 has 2 KB). The
  rammux does not interrupt a burst, so a branch costs the burst latency:
  the 2-clock taken branch is optimistic without the stream buffer
  (est.).
- The SRAM's 10 ns fits a 50 MHz word-a-clock only with the board's
  registered pins; T1F's own S3 measurement (input 12.1 ns) is the guide.

## Tang Nano 20K (GW2AR-18)

Everything fits (20,736 LUT4s, 46 BSRAM; FC is 20%). T1S gains nothing:
its core clock fmax (46 MHz) is below T1F's (48 MHz) and it takes two
clocks a step, so 23 M steps/s against T1F's 33.75 M (measured clocks, est.
rates). FC at 39 MHz and CPI 1.5 is about 25 MIPS against T1F's 6.9.

## Recommendation

Do not build T1SE as a new core.

- If the goal is reading 2 (CPI near 1), the design that results is FC:
  keep working on FC (slimming per docs/fc.md, then c1 and the Spartan-3
  synthesis in ISE). FC at 2.8-3.0k LUTs (est. on S3) plus the T1F system's
  other 1.3k (3,725 - ~2.4k, est.) is about 4.1-4.3k, over 3,840, so the
  slimming is required, not optional.
- If the goal is reading 1 (T1SE = T1S at one clock a step), it halves
  T1S's cycles at no more than T1F's MIPS per clock and the same clock
  limit; only the LUT saving (about 450) is left, and it costs 6-7 block
  RAMs. Not worth it. A middle core (variant B) gets 12.8 MIPS at 25 MHz
  but its LUTs are most of FC's, with 2.5x its CPI.
- T1S itself: keep as an experiment, valuable for a target with few LUTs
  and spare block RAM (none in this project). Shrinking its ROM to ~3
  block RAMs is the one cheap change.

Main risks: S3 timing (T1F's 46 ns internal on the -4 grade; no
16-bit core has been through ISE), block-RAM budget, the RAM machine's
latency on taken branches, and the unexplained FC dhrystone result.

## Not measured

ISE LUT/fmax for any core; T1S/FC on the Spartan-3; Doom and BASIC
programs (BASIC images are identical interpreter images); T1S beyond 4
programs (runs unfinished); variants A-C are models, not RTL.

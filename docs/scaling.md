# A scalable fpga_cpu

One instruction set, implemented at transistor budgets that double from
the 6502's. Every tier runs the same binaries. A tier adds the features
that buy the most speed for their cost, and keeps everything the tier
below had. This is the plan; nothing below T4 is built yet.

The instruction set is the 32-bit one: 16 registers of 32 bits (the pairs
of today's ISA become plain registers), NZCV flags, 16- and 32-bit
encodings (2.8 bytes per instruction on average), a 32-bit address space
(mapped by an MMU onto 20, 24 or 32 physical bits; see below). Every
tier implements all of it, including multiply and divide: a small tier
runs them as internal loops, a large one in hardware. Today's 16 × 16-bit
ISA is the T4 core's prototype.

## Generations

Transistor counts from [Wikipedia](https://en.wikipedia.org/wiki/Transistor_count),
where it lists the chip; console CPUs are listed by the core inside them.

| Gen | Budget | Period CPUs (transistors) | Machines | Tier | Tier vs period CPUs: per clock / equal memory |
|---|---|---|---|---|---|
| 1, late 70s to early 80s | 4–10 K | 6800 (4.1 K), 6502 (4.5 K), RCA 1802 (5 K), 8080 (6 K), 8085 (6.5 K), TMS9900 (8 K), Z80 (8.5 K), 6809 (9 K) | Apple II, Atari 400/800, Commodore 64 and VIC-20, BBC Micro, NES (6502 cores); ZX Spectrum, MSX, Amstrad CPC, TRS-80, Master System, Game Boy (Z80 and kin); TI-99/4A; CoCo, Dragon, Vectrex | **T1** | 3.2× / 1.6× (6502, Z80) |
| 2a, early 80s | 10–20 K | 65C02 (11.5 K), Novix NC4016 (16 K), Z8000 (17.5 K) | Apple IIc and enhanced IIe, PC Engine (65C02 core) | **T2** | 6.3× / 6.3× (65C02) |
| 2b, mid 80s | 20–35 K | 65C816 (22 K), ARM1 (25 K), ARM2 (27 K), 8086/8088 (29 K) | IBM PC and XT, Apple IIGS and SNES (65816), Acorn Archimedes (ARM2) | **T3** | 1.0× / 1.0× (ARM2) |
| 3, mid 80s | 50–140 K | 80186 (55 K), 68000 (68 K), SPARC MB86900 (110 K), 80286 (134 K) | Macintosh, Amiga 500, Atari ST, Mega Drive, X68000, IBM PC/AT, Sun-4 | **T4** | 9.4× / 2.4× (68000) |
| 4, late 80s | 190–310 K | 68020 (190 K), i960 (250 K), 68030 (273 K), 80386 (275 K), ARM3 (310 K) | Mac II, Amiga 1200/3000, Atari TT and Falcon, 386 PCs, Archimedes A5000 | **T5** | 3.3× / 1.7× (386, 68030) |
| 5, early 90s | 1–1.4 M | 80486 (1.18 M), 68040 (1.2 M), R4000 (1.35 M) | 486 PCs, Quadra, Amiga 4000, SGI Indigo | **T6** | 1.5× / 1.5× (486, 68040) |
| 6, mid 90s | ~3 M | PowerPC 601 (2.8 M), Pentium (3.1 M) | Power Macintosh, Pentium PCs | T7, later | |

The last column is the tier's speed over the geometric mean of the CPUs
named, on the 10 integer benchmarks of the Speed section (all but
Dhrystone, which SDCC cannot compile). The 6502 and 65C02 are measured
with llvm-mos and the Z80 with SDCC 4.5 and ucsim (T-states, outputs
identical to fpga_cpu's); ARM2, the 68000, 386, 68030, 486 and 68040 are
models from their instruction timings, assuming every access hits in
cache. *Equal memory* divides by each CPU's clocks per memory access (Z80
and 68000 4, 386 and 68030 2, the rest 1), since at a given memory speed
a 4-clock bus can run its CPU four times faster: that is the fairer
figure for a machine whose memory sets the clock. The 6809, TMS9900,
8086, 65816 and 286 are not measured.

- **T1** beats both, by 2.0× over the 6502 and 1.3× over the Z80 at
  equal memory (the Z80 takes 2.7× the 6502's clocks).
- **T2** is the largest lead: a 16-bit ALU with 16 registers against an
  8-bit CPU.
- **T3** equals ARM2 by construction: it is ARM2's cycle table.
- **T4** and **T5** win mostly on bus clocks: the 68000 needs 4 clocks
  per access and the 386 and 68030 two.
- **T6** keeps a 1.5× lead over the 486 and 68040, from the second pipe.

The 68HC11 and most console chips are not in that table: they are
microcontrollers or custom parts around one of these cores.

## Memory, address space and MMU

**Von Neumann memory, Harvard caches.** There is one flat physical
address space for code, data and I/O, and the CPU fixes nothing in it
except where it starts after reset. What sits at each address (RAM, ROM,
I/O, slow or fast) is the board's choice, decoded outside the CPU.
Instructions and data separate only in the caches. A physical part could
also split the buses so it fetches an instruction and data in the same
cycle, but that costs a second address and data bus, about 50–64 more
pins and their pad drivers, plus die area. Once T5 has an instruction
cache, most fetches never reach the bus, so one bus suffices. The
Spartan-3 board has one SRAM, so it is von Neumann too, with the block
RAM I-cache as its Harvard part.

**Address space.** It is 32 bits in the architecture. Pointers are full
registers, and programs see one 32-bit virtual space on every tier. An
MMU, trivial on small tiers and full on large ones, maps that space
onto the N physical address bits the tier implements (20, 24 or 32).

The architecture fixes only the top of the virtual space. Reset starts
at FFFFFFF0, and the exception vectors are just below it. Every MMU maps
that address to the top of physical memory at reset, so the board puts
its boot ROM at the top of whatever range it decodes. Today's memory map
(docs/architecture.md: RAM from 0, I/O at F00000, boot ROM at FF0000) is
one board's choice of this, not part of the architecture.

### MMU levels

| Level | Tiers | What it does | Est. transistors |
|---|---|---|---|
| **M0**, fold | T1–T3 | Physical = VA[N−1:0], valid only if VA[31:N] are all zeros or all ones. So all of physical memory appears from 0 up, and again at the top, where the vectors are. No registers: two wide gates on the top bits | ~0.1 K |
| **M1**, attributes and protection | T4 | M0, plus a 16-entry table of attributes for the physical address space, one entry per sixteenth of it: execute, write and cacheable bits. Also a user/supervisor bit, and a base-and-limit register that relocates and bounds user-mode addresses | ~3 K (5% of T4) |
| **M2**, paging | T5 | 4 KB pages, two-level page tables walked in hardware, a 32-entry TLB. Each page has read, write, execute, user and cacheable bits. The 386 and 68030 put this on chip in the same generation | ~25–35 K (~10% of T5) |
| **M3**, full | T6 | Separate instruction and data TLBs of 64 entries, address-space IDs so a task switch keeps the TLB, accessed and dirty bits set by hardware | ~60–80 K |

Every level starts out of reset behaving exactly like M0, with every
region executable, writable and uncached. A flat binary therefore runs
on every tier, and only an OS or a boot ROM turns translation and
attributes on. The MMU registers sit in the MFSR/MTSR space and read
as zero where a level does not exist.

**The execute bit lets the board attach memory anywhere.** The
architecture has no fixed code area, I/O area or ROM area. The
attributes tell the CPU how to treat each region instead:

- **No-execute** stops instruction fetch and prefetch at the region.
  This matters from T4 on. The T4 prefetch buffer and the T5 cache read
  ahead of execution, and reading an I/O register can have side effects
  (it can pop a FIFO or clear a status flag). A fetch that crosses into
  a no-execute region waits until execution actually reaches it, then
  faults.
- **Uncacheable** keeps I/O, and memory another bus master writes to
  (video, DMA), out of the caches.
- **Read-only** catches writes to ROM, and to code pages once they are
  loaded.

A tier without that bit (T1–T3, M0) has no prefetch deep enough or cache
to need it. On those tiers, keeping code away from I/O registers is the
board's job, as it was on the 6502.

**Other points:**

- **The address check is cheap, and it stops tags.** The 68000 ignored
  its top 8 address bits. Early Mac software stored flags in them, and
  that software broke on the 68020 (the "32-bit clean" problem). Every
  level here faults on an address outside the two folds, so software
  cannot hide data in the top bits.
- **T1's address arithmetic stays short.** A 20-bit tier computes an
  effective address in 3 byte-wide ALU passes instead of 4. The top byte
  only has to be checked, from the base's top bits, the displacement's
  sign and the carry out of bit 19, not computed. Every load or store
  with a displacement is a cycle shorter.
- **T5's cache and pages fit each other.** A 4 KB direct-mapped cache
  indexes only within the 4 KB page offset. So the TLB lookup runs in
  parallel with the cache read, and the tag compare uses the physical
  address. T6's 16 KB cache keeps this by being 4-way set associative.
- **A data cache is the next Harvard step.** The tiers above assume
  memory that keeps up with the core, as the board's 10 ns SRAM does at
  50 MHz, so none of them has a D-cache. A part whose memory falls
  behind its clock adds one beside the I-cache. The uncacheable bit is
  already there for it, and SYNC already flushes the I-cache after code
  is written.

What each physical width saves, estimated:

| Bits | Physical reach | Saves against 32 bits | Where it pays |
|---|---|---|---|
| 20 | 1 MB, shared by RAM, ROM and I/O as the board decides | 12 bits of PC, incrementer, address latch and pins: ~0.7 K transistors | T1: ~10% of the chip. A part with 20 address pins and 8 data pins fits a 40-pin DIP, like the 8088 |
| 24 | 16 MB | 8 bits: ~0.5 K, and 8 bits off every cache tag | T2 to T4 (the 68000 and 65816 had 24 address bits). Needs a 64-pin package |
| 32 | 4 GB | — | T5 and up. On T5's 4 KB cache with 16-byte lines, the 8 extra tag bits per line are 2 K bits, ~12 K transistors (4% of T5), small next to the gain |

Registers, the ALU and pointer arithmetic stay 32 bits at every width.
Only the parts that hold or drive a physical address shrink. A binary
built for a 32-bit tier runs on a 20-bit one if its data fits, and
faults cleanly if it does not.

## The tiers

Each tier adds to the one before. Transistor counts are design estimates
in static CMOS (a 6502 in CMOS, the 65C02, is 11.5 K); FPGA cores map to
the same tiers.

| Tier | Adds | Est. transistors | FPGA core |
|---|---|---|---|
| **T1** | 8-bit ALU and bus; the 16 × 32-bit registers on chip (one read and one write a cycle); hardwired control; shifts, multiply and divide as 1-bit-per-step loops; MMU M0 (fold); 20 physical address bits | ~7 K | — |
| **T2** | 16-bit ALU and bus; a second register read port; a 4-byte prefetch queue overlapping fetch with register-only work; 24 physical address bits | ~12–14 K | — |
| **T3** | 32-bit ALU and bus; barrel shifter; 3-stage pipeline; Booth multiply 2 bits a cycle (ARM2's cheap multiplier; ARM1 had none) | ~25 K | — |
| **T4** | 5-stage pipeline with forwarding; 1-cycle taken branch and CMP/ALU+Bcc fusion; 16-byte prefetch buffer; 16 × 16 multiplier array; a divider running beside the pipe; MMU M1 (region attributes: execute, write, cacheable; supervisor mode; one user region) | ~55–75 K | c1 |
| **T5** | 4 KB instruction cache (instruction fetch no longer waits for data); 32 × 32 multiplier; MMU M2 (paging, 32-entry TLB); 32 physical address bits | ~320 K | c2 |
| **T6** | a second, simple pipe; a second register write port; radix-4 divider; 16 KB 4-way instruction cache; MMU M3 (split TLBs, address-space IDs) | ~1.2–1.3 M | c3 |

Where the transistors go:

- **T1:** registers 512 bits ≈ 3.5 K (cells and decoders), ALU ≈ 0.5 K,
  PC and address ≈ 0.6 K, instruction register and decode ≈ 1.5 K,
  sequencer and bus ≈ 1 K. The registers are half the chip, and the
  biggest single speedup: with registers in memory (TMS9900 style) every
  instruction would make three extra memory trips.
- **T3:** 32-bit ALU ≈ 2 K, barrel shifter ≈ 2.5 K, two-read-port
  register file ≈ 5.5 K, pipeline ≈ 2 K, decode ≈ 4 K. The same class as
  ARM1/ARM2, which is the check on these numbers.
- **T5:** the cache is the cost: 32 K bits ≈ 200 K transistors plus tags.
  ARM3 (ARM2 plus a 4 KB cache, 310 K) is the check.

On an FPGA the cache is a block RAM and costs no logic, which is why the
FPGA ladder (docs/cores.md) has it before the second pipe, where the
transistor ladder has it after.

## Speed

Measured on the 6502 with llvm-mos 23.3 and its simulator (`mos-sim
--cycles`), our C benchmarks with 16-bit `int`. The tiers are modelled:
32-bit ARM code (16 registers, the same shape as the new ISA) built with
gcc -O2, its executed instruction classes counted in QEMU, priced with each
tier's cycles per class (single-cycle memory, like the 6502's). Millions of
cycles:

| Benchmark | 6502 | T1 | T2 | T3 | T4 | T5 |
|---|---|---|---|---|---|---|
| CoreMark (10 iterations) | 113.6 | 40.2 | 12.3 | 5.7 | 4.2 | 3.9 |
| Dhrystone (10,000) | 48.9 | 35.8 | 11.2 | 5.9 | 4.6 | 4.2 |
| Richards | 85.1 | 55.2 | 17.6 | 10.4 | 7.0 | 6.1 |
| Sieve | 24.2 | 12.2 | 3.0 | 2.1 | 1.6 | 1.5 |
| Stanford Bubblesort | 56.4 | 27.5 | 9.2 | 5.4 | 4.3 | 3.9 |
| Stanford IntMM | 108.8 | 37.4 | 12.7 | 2.5 | 1.3 | 1.1 |
| Stanford Perm | 62.8 | 29.3 | 9.8 | 5.2 | 4.2 | 3.7 |
| Stanford Puzzle | 80.8 | 54.3 | 14.2 | 9.3 | 6.9 | 6.4 |
| Stanford Queens | 147.0 | 100.0 | 28.0 | 15.9 | 12.6 | 11.6 |
| Stanford Quicksort | 73.8 | 34.5 | 9.8 | 5.1 | 3.9 | 3.7 |
| Stanford Towers | 58.6 | 49.5 | 16.4 | 8.2 | 6.3 | 5.7 |
| **Faster than the 6502, per clock (geomean)** | 1 | **1.8×** | **6.1×** | **12×** | **16×** | **18×** |
| Cycles per instruction (model) | | 13.2 | 4.0 | 2.0 | 1.5 | 1.35 |

T6 adds about 1.25× to T5 (measured, c2 to c3): about 23× the 6502. All 11
programs printed the same output on the 6502 as on fpga_cpu. Left out:
Treesort (its tree does not fit in 64 KB), Whetstone (llvm-mos has no
`sin`/`cos`), and the floating-point Stanford programs (the ARM proxy uses
a hardware FPU, which none of the tiers has).

Checks on the model:

- Today's c0 core, measured, is 10× the 6502 on the same programs
  (geomean; from 4.9× on Dhrystone to 31× on IntMM). The model's T4
  (c1 timing plus the 32-bit ISA) at 16× is in line: c1 adds 1.17×, and
  the 32-bit registers remove the carry halves and spills (docs/cores.md,
  and the ARM register-count measurement).
- T3's cycle table is ARM2's (load 3, store 2, taken branch 3, Booth
  multiply), so T3 is ARM2 running ARM code. The ISA's denser encoding
  is not counted.

Against later generations, on Doom (320×200, per frame): a 386, modelled
from its instruction timings, needs about 9.0 M clocks running gcc's code;
today's c2 core needs 5.4 M (measured, 16-bit ISA). An ideal PowerPC 601
needs 2.6 M; today's c3 needs 4.5 M.

## What the ISA must keep cheap for T1

- 2-operand 16-bit forms (`rd = rd op rs`), so one read and one write per
  cycle are enough.
- Register fields of 4 bits and a regular layout, so decoding is a small
  hardwired table rather than microcode.
- Multiply, divide and variable shifts as instructions at every tier, so
  binaries never change; T1 loops, T4 and up have hardware.
- No memory-to-memory operations or complex addressing beyond base +
  displacement, base + index × scale and post-increment.

## Next measurements

- Put T1–T3 timing into fsim (datapath width, bus width, register ports,
  prefetch), so the tiers run real binaries instead of a model.
- The same programs on more period CPUs: 6809, 8086 and 65816 measured
  in a simulator, and the 68000 and ARM2 with real code instead of models.

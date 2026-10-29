# Core configurations

The CPU is meant to scale: one instruction set, and a ladder of cores that
add hardware in order of speed per LUT. Every rung is a set of choices that
the simulator, the compiler and (later) the RTL take as parameters, so a
bigger FPGA turns more of them on without changing any software.

| Core | Adds | `fsim` | `fcc` |
|---|---|---|---|
| c0 | 2 KB I-cache, 2-cycle taken branch, single issue | `--core c0` | `--cpu=generic` |
| c1 | 1-cycle taken branch; CMP+Bcc and ALU+Bcc fusion | `--core c1` | `--cpu=fcpu1` |
| c2 | 4 KB I-cache (one more block RAM) | `--core c2` | `--cpu=fcpu1` |
| c3 | a second, simple pipe: ALU, MOVI, branch, shift and pointer ops (`MOVP`, `ADDP*`, `LEA`); either instruction of a pair may use it; a second register write port | `--core c3` | `--cpu=fcpu2` |

`--core` sets the timing options below; options after it override parts of
it, so any combination can be measured (`--core c3 --icache 2048` is c1
plus the second pipe):

| Choice | fsim option | Rung |
|---|---|---|
| taken-branch penalty | `--branch-pen 1` | c1 |
| branch fusion | `--fuse=cmpbr,alubr` | c1 |
| I-cache size | `--icache 4096` | c2 |
| second pipe | `--dual=ports --dual-any --dual-ops=alu,movi,branch,shift,ptr` | c3 |

The compiler side is a scheduling model per core
(`llvm/target/FCPU/FCPUSchedule.td`), chosen with `fcc --cpu`:

- `generic` keeps source order: the original compiler, unchanged (its
  output is byte-identical to before the models existed).
- `fcpu1` is single issue and knows a load's result arrives one cycle
  late, so it fills the load-use slot.
- `fcpu2` adds the two pipes: a full pipe, and the simple pipe that takes
  register-only ALU, shift, MOVI and pointer ops and branches.

Both models keep a flag-setting instruction next to its `Bcc` (a macro
fusion mutation) so the branch fusion of c1 and up still applies, and both
schedule before and after register allocation. Scheduling before
allocation sometimes stretches a loop's pointer increments past the loads
that use the old pointer, which costs a `MOVP` per pointer per iteration
(seen in `memcpy` and `strcpy`; up to 3–4% more instructions in BASIC and
Doom). It still wins overall (C: 1.506× against 1.495× scheduling after
allocation only); `fcc` with `FCC_MLLVM=-fcpu-prera-sched=false` turns it
off for experiments.

## Measured (variant A)

`benchmarks/run.py --doom --configs c0,c0s,c1g,c1,c2,c1d,c3g,c3s1,c3`.
378 runs, all with correct output. Speedup is cycles against c0 with
generic code, geomean over the programs of each suite; "all" is the
geomean of the three suites, so Doom counts as one workload of three.
Instructions are relative to generic code.

| Config | Core | Code | All | C (32) | BASIC (9) | Doom | Doom fps | CPI C / BASIC / Doom | Instructions C / BASIC / Doom |
|---|---|---|---|---|---|---|---|---|---|
| c0 | c0 | generic | 1.000× | 1.000× | 1.000× | 1.000× | 6.29 | 1.333 / 1.745 / 1.389 | 1.000 / 1.000 / 1.000 |
| c0s | c0 | fcpu1 | 1.018× | 1.030× | 1.015× | 1.008× | 6.31 | 1.291 / 1.670 / 1.359 | 1.002 / 1.029 / 1.014 |
| c1g | c1 | generic | 1.144× | 1.171× | 1.142× | 1.120× | 7.06 | 1.138 / 1.528 / 1.240 | 1.000 / 1.000 / 1.000 |
| **c1** | c1 | fcpu1 | **1.177×** | 1.221× | 1.170× | 1.142× | 7.14 | 1.090 / 1.448 / 1.200 | 1.002 / 1.030 / 1.014 |
| **c2** | c2 | fcpu1 | **1.244×** | 1.238× | 1.299× | 1.196× | 7.51 | 1.074 / 1.305 / 1.146 | 1.002 / 1.030 / 1.014 |
| c1d | c1 + second pipe | fcpu2 | 1.416× | 1.506× | 1.359× | 1.387× | 8.73 | 0.881 / 1.248 / 0.965 | 1.004 / 1.029 / 1.039 |
| c3g | c3 | generic | 1.453× | 1.453× | 1.480× | 1.427× | 9.02 | 0.917 / 1.179 / 0.973 | 1.000 / 1.000 / 1.000 |
| c3s1 | c3 | fcpu1 | 1.509× | 1.515× | 1.530× | 1.482× | 9.33 | 0.878 / 1.107 / 0.925 | 1.002 / 1.030 / 1.014 |
| **c3** | c3 | fcpu2 | **1.528×** | 1.535× | 1.526× | 1.522× | 9.62 | 0.865 / 1.111 / 0.879 | 1.004 / 1.028 / 1.039 |

What each part buys:

- **Compiler scheduling** (no LUTs): +2–3% on c0 and c1, +5% on c3.
  The dual-issue model helps Doom most (pairs); for BASIC the
  single-issue model is as good.
- **1-cycle branch + fusion** (c1): +14% from the hardware alone.
- **4 KB I-cache** (c2): +6% overall, +11% for BASIC, whose interpreter
  loop does not fit in 2 KB. With the second pipe it is worth more
  (c1d → c3: +8%), because fetch stalls are a larger share of fewer cycles.
- **Second pipe** (c3): +23% over c2. CPI drops below 1 for C and Doom.

## Speed per LUT (estimates)

LUT figures are design estimates, not synthesis results; replace them
when the RTL exists. The c0 core (without video, I/O and the blitter) is
assumed to be about 1.0–1.2k LUT4. MIPS counts c0's instructions (useful
work), at 50 MHz: 50 × speedup / 1.475, the geomean CPI of c0 over the
three suites.

| Core | Step adds (LUTs) | Block RAMs | Core total (LUTs) | All | MIPS | MIPS per 1k LUTs | Step: speedup % per 100 LUTs |
|---|---|---|---|---|---|---|---|
| c0 | – | I-cache 1 | 1,000–1,200 | 1.000× | 33.9 | 30.8 | – |
| c1 | 70–160 | – | 1,070–1,360 | 1.177× | 39.9 | 32.8 | 15.4 |
| c2 | 40–90 (tags) | +1 | 1,110–1,450 | 1.244× | 42.2 | 33.0 | 8.8 |
| c3 | 600–920 | – | 1,710–2,370 | 1.528× | 51.8 | 25.4 | 3.0 |

Step costs:

- c1: branch target adder and redirect in decode (30–80), plus flag
  forwarding and pair detect for fusion (40–80).
- c2: the second I-cache block RAM and twice the tag store.
- c3: second decoder, 16-bit ALU, register-file read copies, forwarding,
  pairing check and flag merge (300–450); a second write port (60–120);
  steering either instruction to either pipe (40–80); the slot-2 16-bit
  shifter (60–90); and a 32-bit pointer adder with pair-wide reads
  (100–180).

c1 and c2 raise speed per LUT; c3 is the fastest core but costs a quarter
of the speed per LUT. On the XC3S200 (3,840 LUTs, 11 of 12 block RAMs
already planned) c1 fits, c2 needs the twelfth block RAM, and c3 probably
does not fit beside the video. It belongs on the next board
([architecture.md](architecture.md)).

## Not on the ladder

- **8-bit instructions** and **accumulator or stack encodings**: measured
  over the same programs, an 8-bit form of today's register instructions
  covers 11–14% of instructions and saves 5–6% of fetch bytes using half
  of the first-byte code space: at most about 1–2% speed (BASIC), about 0
  for C. An implicit accumulator or top of stack would make neighbouring
  instructions depend on each other, which is what the second pipe needs
  them not to do.
- **Variant B** and **extension X** are separate axes; they combine with
  any core.

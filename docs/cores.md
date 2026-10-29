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
| c3 | a second, simple pipe: ALU, MOVI, branch, shift and pointer ops (`MOVP`, `ADDP*`, `LEA`); either instruction of a pair may use it; a second register write port | `--core c3` | `--cpu=fcpu1` (`fcpu2` to be retuned) |

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

- `generic` is the default compiler's model (`FCPUModel` in `FCPU.td`):
  single issue, a load's result one cycle late, no per-instruction
  classes, and the post-RA list scheduler.
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

`benchmarks/run.py --doom --configs c0,c0s,c1g,c1,c2,c1d,c3g,c3s1,c3`,
with the compiler of commit 500a352 (hoisted global loads, subregister
liveness, the `FCPUModel` schedule for `generic`). 810 runs (variants A
and B), all with correct output; the table is variant A. Speedup is cycles
against c0 with generic code, geomean over the programs of each suite;
"all" is the geomean of the three suites, so Doom counts as one workload
of three. Instructions are relative to generic code.

| Config | Core | Code | All | C (32) | BASIC (9) | Doom | Doom fps | CPI C / BASIC / Doom | Instructions C / BASIC / Doom |
|---|---|---|---|---|---|---|---|---|---|
| c0 | c0 | generic | 1.000× | 1.000× | 1.000× | 1.000× | 7.58 | 1.307 / 1.720 / 1.353 | 1.000 / 1.000 / 1.000 |
| c0s | c0 | fcpu1 | 1.002× | 1.009× | 0.996× | 0.999× | 7.58 | 1.289 / 1.726 / 1.341 | 1.005 / 1.000 / 1.010 |
| c1g | c1 | generic | 1.143× | 1.173× | 1.128× | 1.127× | 8.54 | 1.114 / 1.524 / 1.201 | 1.000 / 1.000 / 1.000 |
| **c1** | c1 | fcpu1 | **1.172×** | 1.206× | 1.156× | 1.155× | 8.77 | 1.079 / 1.488 / 1.160 | 1.005 / 1.000 / 1.010 |
| **c2** | c2 | fcpu1 | **1.237×** | 1.218× | 1.280× | 1.214× | 9.23 | 1.068 / 1.343 / 1.104 | 1.005 / 1.000 / 1.010 |
| c1d | c1 + second pipe | fcpu2 | 1.366× | 1.474× | 1.284× | 1.345× | 10.20 | 0.882 / 1.338 / 0.984 | 1.005 / 1.001 / 1.023 |
| c3g | c3 | generic | 1.468× | 1.470× | 1.482× | 1.452× | 11.01 | 0.889 / 1.161 / 0.932 | 1.000 / 1.000 / 1.000 |
| **c3s1** | c3 | fcpu1 | **1.468×** | 1.463× | 1.470× | 1.472× | 11.22 | 0.889 / 1.169 / 0.910 | 1.005 / 1.000 / 1.010 |
| c3 | c3 | fcpu2 | 1.454× | 1.495× | 1.440× | 1.429× | 10.87 | 0.870 / 1.194 / 0.926 | 1.005 / 1.001 / 1.023 |

What each part buys:

- **Compiler scheduling** (no LUTs): with `generic` now scheduling for
  load latency itself, `fcpu1` adds about 2.5% on c1 and c2 (mostly by
  keeping CMP+Bcc together for fusion) and nothing on c0.
- **The dual-issue model `fcpu2` currently loses** on c3: it pairs more
  in C (CPI 0.870) but runs 2.3% more instructions in Doom and pairs
  worse in BASIC, so c3 is fastest with `generic` or `fcpu1` (1.468×).
  Before 500a352 it was the best (1.528× against 1.453× for generic);
  most of that gain is now in `generic`. `fcpu2` needs retuning against
  the new passes before it earns its place.
- **1-cycle branch + fusion** (c1): +14% from the hardware alone.
- **4 KB I-cache** (c2): +6% overall, +11% for BASIC, whose interpreter
  loop does not fit in 2 KB.
- **Second pipe** (c3): +19% over c2. CPI drops below 1 for C and Doom.

## Speed per LUT (estimates)

LUT figures are design estimates, not synthesis results; replace them
when the RTL exists. The c0 core (without video, I/O and the blitter) is
assumed to be about 1.0–1.2k LUT4. MIPS counts c0's instructions (useful
work), at 50 MHz: 50 × speedup / 1.449, the geomean CPI of c0 over the
three suites. Each core uses its best measured code (c3: `fcpu1`).

| Core | Step adds (LUTs) | Block RAMs | Core total (LUTs) | All | MIPS | MIPS per 1k LUTs | Step: speedup % per 100 LUTs |
|---|---|---|---|---|---|---|---|
| c0 | – | I-cache 1 | 1,000–1,200 | 1.000× | 34.5 | 31.4 | – |
| c1 | 70–160 | – | 1,070–1,360 | 1.172× | 40.4 | 33.3 | 15.0 |
| c2 | 40–90 (tags) | +1 | 1,110–1,450 | 1.237× | 42.7 | 33.3 | 8.5 |
| c3 | 600–920 | – | 1,710–2,370 | 1.468× | 50.7 | 24.8 | 2.5 |

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

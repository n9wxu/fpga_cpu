# Benchmarks

Classic benchmarks and real programs for measuring and tuning the
fpga_cpu ISA: every one is compiled for the CPU, run in the simulator
under different timing models, checked against the same program built
for the development machine, and timed in cycles.

```sh
./run.py                     # everything except Doom: variants A and B, six timing configurations
./run.py --quick             # base configuration only
./run.py dhrystone stanford  # some of them
./run.py doom                # Doom (needs doom/get_wad.sh first)
./run.py --list
./run.py --doom --configs c0,c1,c2,c3   # the core ladder (docs/cores.md)
```

Results go to `results.md` (tables) and `results.json` (every run). The
runner needs the LLVM toolchain (`llvm/setup.sh`), the simulator (`make
-C sim`) and, for the BASIC programs, the BASIC target build (it makes
that itself).

## What's here

| Directory | Benchmark | Origin |
|---|---|---|
| `c/dhrystone` | Dhrystone 2.1 | R. P. Weicker, 1988 (netlib sources) |
| `c/whetstone` | Whetstone | Curnow & Wichmann, 1972; C version by R. Painter, 1998 |
| `c/richards` | Richards | Martin Richards, BCPL; C from the LLVM test suite |
| `c/stanford` | Stanford integer and float suite: Bubblesort, IntMM, Perm, Puzzle, Queens, Quicksort, Towers, Treesort, FloatMM, RealMM, Oscar | John Hennessy, early 1980s; LLVM test-suite copies |
| `c/sieve` | BYTE Sieve of Eratosthenes | J. & G. Gilbreath, BYTE, 1981 |
| `c/coremark` | CoreMark 1.0 | EEMBC, 2009, with an fpga_cpu port in `fcpu/` |
| `basic/DHRY.BAS` | Dhrystone 2.1 in BASIC | translated here from the C version |
| `basic/BM1.BAS`–`BM8.BAS` | Rugg/Feldman BASIC benchmarks | Kilobaud, June 1977 |
| `doom` | Doom via doomgeneric | id Software 1993, GPL v2; see `doom/README.md` |

Each directory keeps its upstream license and, where it came from a
repository, the commit in `UPSTREAM`.

## How the programs were adapted

Changes to the sources are few and marked `fpga_cpu:`:

- **Size.** Copies from the LLVM test suite are scaled for modern
  machines (Richards runs a million passes, Stanford repeats 100 times).
  Here the size is a build-time setting (`ITERS`, `CLASSIC_SIZE`,
  `LOOPS`, `NUMBER_OF_RUNS`, `ITERATIONS`), chosen so a run takes roughly
  1–30 million cycles and a sweep over configurations stays quick.
- **Timing.** The runtime's `clock()` counts CPU cycles / 16, so
  Dhrystone and Whetstone time themselves exactly even on short runs
  (`FCPU_EXACT_TIMER` drops their 2-second minimum). The runner also
  records the simulator's total cycles for every run.
- **Input.** No keyboard input: Dhrystone's run count is built in.
- **Exit status.** Dhrystone's K&R `main` gained a `return 0`; it fell
  off the end, so the exit status was garbage.
- **CoreMark** runs too briefly to be a reportable CoreMark score (EEMBC
  asks for 10 seconds). Its iterations per second per MHz are an ISA
  measurement here, nothing more. The core files are unmodified.

## int width

fpga_cpu's C has a 16-bit `int`; `fcc --int32` gives a 32-bit `int` for
code written for 32-bit machines. The C benchmarks are built both ways
where they work both ways, which measures what a 16-bit `int` buys on
this CPU. The Stanford programs assume a 32-bit `int` (their random
numbers and data overflow 16 bits), so their 16-bit builds print
different numbers from the reference; those builds are checked by the
programs' own `Error ...` tests instead. CoreMark is checked against its
published CRCs for the performance seeds, at either width.

## BASIC

The BASIC programs run in the fpga_cpu BASIC interpreter (`basic/`),
itself compiled for the CPU, so they measure the interpreter's code on
the ISA. They read the cycle counter with `DEEK(&HF00004)` and
`DEEK(&HF00006)`. `DHRY.BAS` checks its own results the way the C
Dhrystone does, and prints DMIPS.

## Arithmetic without an FPU

Doom uses no floating point (it's 16.16 fixed point), and the float
benchmarks use soft float, so both depend on the runtime's 32/64-bit
multiply, divide and soft-float routines. Profiling with `fsim --profile`
showed most of the cost was in compiler-rt's generic C helpers, so these
are now hand-written in `llvm/runtime/asm/` (see `llvm/README.md`).
Variant A, base timing:

| benchmark | original | hand-written runtime | + extension X |
|---|---|---|---|
| Doom timedemo | 4.77 fps | 6.25 fps | 7.01 fps |
| Whetstone, int16 | 18.75 M cycles | 5.95 M (3.15×) | 5.20 M (3.61×) |
| Stanford FloatMM, int16 | 98.9 M | 32.2 M (3.07×) | 30.0 M (3.30×) |
| Stanford Oscar, int32 | 78.7 M | 23.9 M (3.29×) | 22.3 M (3.52×) |
| BASIC benchmark 8 | 25.6 M | 15.6 M (1.64×) | 14.9 M (1.72×) |

Per operation: float add 280 → 107 cycles, multiply 271 → 82, divide
1,335 → 394; Doom's `FixedMul` about 250 → 80 and `FixedDiv` about
4,900 → 550.

Extension X ([cpu/isa.md](../cpu/isa.md)) adds four candidate
instructions. Measured one at a time (configurations `x-*`, geomean over
all benchmarks, variant A):

| instruction | geomean speedup | Doom | est. extra LUTs |
|---|---|---|---|
| `DSTEP` | 1.022× | 1.107× | 70–120 |
| `MULPU` | 1.004× | 1.012× | 15–30 |
| `FSHR`/`FSHL` | 1.003× | 1.004× | 40–90 |
| `CLZ` | 1.000× | 1.000× | 25–40 |

Only `DSTEP` pays for itself, at about 2–3% per 100 LUTs. That is close
to dual issue and far below a 1-cycle taken branch (`bp1`, ~25% per 100
LUTs; docs/measurements.md). What remains in Doom is mostly compiler
code: 32-bit multiplies are calls to `__mulsi3`, and the column and span
loops reload globals on every pixel.

# sid6502: a 6502 interpreter and dynamic recompiler for fpga_cpu

Plays C64 `.sid` music routines on the fpga_cpu CPU. A `.sid` file is a 6502 routine
ripped from C64 memory; a player loads it into a 64 KB 6502 address space, calls `init(song)`
once and `play()` at 50 Hz (or any rate), and the routine writes the SID registers
`$D400-$D41F`. This program runs that code and records every SID write as
`(6502 cycle, register, value)` so a separate audio engine can replay it.

Two engines run the same 6502 and share one state and one memory image:

- **Interpreter** (`src/interp.c`): plain C, exact cycles, every documented and stable
  undocumented opcode. The reference, and the JIT's fallback.
- **JIT** (`src/jit.c`, `src/jit_rt.s`): translates 6502 traces to fpga_cpu code at run
  time, links them, and leaves to C only for the rare things it does not do natively.

Status: both engines agree with an independent Python reference on 3,175 tests (below),
cycle for cycle. See the end for measured speed and limits.

```sh
make -C apps/sid6502 host     # interpreter vs the Python reference, on the host (seconds)
make -C apps/sid6502 test     # interpreter + JIT (+ stepwise JIT) in fsim, 3,175 tests (about 3 minutes, CORE=c0|c1)
make -C apps/sid6502 test-small   # same with a tiny translation cache (forces flushes)
make -C apps/sid6502 bench    # speed on synthetic music routines, cores c0 and c1
```

`make` needs `llvm/fcc` and `sim/build/fsim` (see llvm/README.md, sim/README.md) and Python 3.
Nothing outside this directory is changed.

## Files

| Path | Contents |
|---|---|
| `src/sid6502.h` | Public API: state, SID log, hooks, `call_routine`, `jit_run`, `interp_run` |
| `src/interp.c` | Interpreter, memory model, I/O hooks, SID log |
| `src/jit.c` | Translator (decode, flag liveness, emitter), cache, linker, dispatcher, event handlers |
| `src/jit_rt.s` | `jit_enter` / `jit_exit` / `jit_sync`: the only assembly |
| `src/optable.h` | Generated from `tools/m6502.py` (`tools/gen_optable.py`) |
| `src/shadow.h`, `src/stub_host.c` | Test plumbing: a second renamed interpreter; host stubs |
| `tools/m6502.py` | The opcode table (single source) and a two-pass 6502 assembler |
| `tools/ref6502.py` | Independent Python reference CPU (decimal mode, undocumented ops, exact cycles) |
| `tools/dumptest.py`, `trace_ref.py` | Disassemble / trace one generated test |
| `tools/mkmusic.py` | Writes the benchmark routines `MUSIC0/2K/5K.BIN`, `DIGI.BIN` |
| `tests/gen_tests.py` | Generates the test vectors (`tests/disk/TESTS*.BIN`) from the Python reference |
| `tests/testrun.c` | Test runner (host: interpreter; fsim: interpreter, JIT, stepwise JIT) |
| `tests/bench.c` | Speed measurement |

Programs are built with `fcc --base 0x100000` and run with `fsim --xram 4 --load 0x100000`
because the arrays below do not fit in the 333 KB below the video buffers of the base map.
For the base map, `-DCODE_WORDS=20000 -DMAX_EV=1000 -DSID_LOG_MAX=1024` brings BSS to 240 KB and
works at the default `--base 0x1000` (checked with `bench.c`).

## Memory

| Array | Size | Use |
|---|---|---|
| `mem6502` | 64 KB | the 6502's RAM (all of it is RAM; nothing is banked) |
| `attr6502` | 64 KB | one byte per address: bit 0 = byte of translated code, bit 1 = `$D000-$DFFF` |
| code cache | 88 KB (`CODE_WORDS`) | translated code; one event-table entry per exit (`MAX_EV` = 2,400), 2,048-entry hash |
| `sidlog` | 32 KB (`SID_LOG_MAX` = 4096) | the SID write log |
| `wsite`, `vol` | 2 x 1 KB | bit maps: static store targets, "volatile" code bytes |

## API (`sid6502.h`)

```c
sid_reset();                                   // clear memory and state
sid_load(load_addr, data, len);                // copy the tune in (flushes translations)
cpu.s = 0xFF;
call_routine(init, song, 0, 0, max_cycles, 1); // 1 = JIT, 0 = interpreter; A,X,Y as arguments
for each frame {
    cpu.cycles = max(cpu.cycles, frame * 19656);   // idle time; PAL, any rate works
    call_routine(play, 0, 0, 0, max_cycles, 1);
}
// sidlog[0..sid_n) = (cycle, reg, val); consume, then sid_log_clear()
```

`call_routine` pushes `STOP_PC-1` ($FFEF) as the return address, so the routine's `RTS` stops the
run. `cpu.cycles` is the 6502 clock (32 bits, 71 minutes at 1 MHz). A routine that ends in
`RTI` or `JMP $EA31` (RSID style) is not supported yet. Status: `RUN_STOP`, `RUN_LIMIT` (cycle
limit hit, to the granularity of one trace), `RUN_ILLEGAL` (cpu.pc is the offending opcode).

SID writes: any store to `$D400-$D7FF` is logged with `reg = addr & 31`. The timestamp is the
cycle of the instruction's last access: cycles before the instruction + its cycle count - 1
(for read-modify-write only the final write is logged). `io_read_hook` / `io_write_hook` let the
caller see or serve other I/O; the default read hook returns a raster counter for `$D011/$D012`
(63 cycles a line, 312 lines) and otherwise the last value stored.

## JIT design

### Register map (inside translated code)

| fpga_cpu | 6502 | |
|---|---|---|
| r0, r1 | T0, T1 | scratch (a pair, so they can also hold an event number) |
| p1 (r2,r3) | | `mem6502 + 0x8000` |
| p2 (r4,r5) | | `attr6502 + 0x8000` |
| r6, r7, r8 | A, X, Y | always 0..255 |
| r9 | S | 0..255 (the stack byte is at `0x100 + S`) |
| r10 | RES | N and Z: `N = bit 7`, `Z = (RES == 0 or RES >= 0x100)`. Values >= 0x100 are the "special" form, which is how N=1 and Z=1 together (`BIT`) are kept |
| r11 | C | 0 or 1 |
| r12 | V | bit 7 |
| r13 | CYC | cycle budget, counts down |
| p7 (sp) | | the C stack, never touched |

6502 addresses are folded onto signed 16-bit offsets: with the base at `mem + 0x8000`, a
constant address is `LDBU rd,[p1 + (addr - 0x8000)]` (one instruction), and a computed address
is `addr XOR 0x8000` in a register, used as a sign-extended index `[p1 + t0]`. `abs,X` is
therefore `ADDI t0,X,(abs+0x8000)` + `LDBU` (wrap at 64 KB is the 16-bit wrap).

D and I are not registers: they are **known at translation time** (part of the cache key
`(pc, D, I)`), set by `SED/CLD/SEI/CLI` in the trace and carried by each exit. `ADC/SBC` with D=1
are therefore decided when the code is generated.

### Translation unit

A **trace** is straight-line code from an entry pc, up to 32 instructions, that continues:
through not-taken forward branches (the taken side is a side exit with its own stub), through
`JMP abs` and `JSR` (the callee is inlined; the return address is pushed), and ends at an
`RTS` (dynamic exit through the dispatcher), a backward branch (both edges leave; the taken one is
assumed hot and pre-charged), an interpreter-only instruction, or the size limit. A trace has one
entry; code reached from the middle starts a new trace (tail duplication).

Pipeline: (1) decode the trace; (2) mark its bytes in `attr6502`, find the flag
liveness; (3) emit code; (4) cold stubs after the body; (5) link.

### Lazy flags

N/Z are kept as a *symbolic source register* while translating (`LDA` leaves "N/Z come from
A", no code), materialised into RES only where needed. C and V are computed only if live. Liveness
runs backwards over the trace and, at every exit, looks at the 6502 code at the target (a bounded
scan, `live_at`) to see which flags are read before being rewritten; dead flags are not computed.
Examples: `CMP #n; BNE` becomes `CMPI; BNE` (flags consumed by the branch directly);
`CLC; ADC` folds the carry into the immediate; `CLC; BCC` is folded to a jump. Exits that
need a flag canonical (N/Z in RES, C in C) pay for it in the exit's stub, not on the hot path.

### Cycle counting

Each trace charges its whole main-path length `K` once, at entry: `ADDI CYC,CYC,-K; BMI over`.
Side exits and the not-taken edge of a loop branch refund the difference in their stubs.
Page-crossing penalties of indexed loads are computed at run time and charged straight to CYC.
Taken branches and their page-cross cost are static. The timestamp of an event inside a trace is
`base + (budget - CYC) - K + kb`, `kb` = static cycles before the instruction. CYC is a 16-bit
down counter refilled by the dispatcher (at most 0x4000 cycles at a time), so a trace is run only
if its cost fits (otherwise the dispatcher re-enters with a bigger budget): the cycle limit is
therefore exact to one trace. Counts are exact: every test compares the final cycle count.

### Cache, chaining, invalidation

- Cache key `(pc, D, I)` in a 2,048-entry hash. Exits are `MOVI T1,event; B jit_exit` stubs;
  when the target gets translated, the stub's `MOVI` is overwritten with `B AL target` (or the
  conditional branch itself is retargeted when the stub has no work), so hot loops run without
  returning to C. `SYNC` is executed before entering after any code change.
- **Self-modifying code.** Every byte of translated code has `ATTR_CODE`. A store that can hit
  such a byte takes the interpreter: static stores are classified at translation time
  (`STA label+1` in translated code is not emitted as a plain store), computed stores check
  `attr6502[addr]` first (`LDBU t1,[p2+t0]; CMPI; BNE stub`). The interpreter's own writes check
  it too. A hit **flushes the cache** and marks the byte "volatile": from then on any instruction
  covering a volatile byte is run by the interpreter each time, which reads memory fresh and so
  is always right. Code translated later at an address a translated store site targets is also made
  volatile (`wsite`), which avoids flush storms for the usual "patch an operand" pattern.
  Translated code and its data can share a page (only the exact bytes are marked).
- Code in page 1 and in `$D000-$DFFF` is never translated (interpreted).
- The cache is flushed when full (code space, event table or hash).

### What the JIT runs natively, and what it hands to the interpreter

Native: LDA/LDX/LDY, STA/STX/STY, ADC/SBC (binary), AND/ORA/EOR, CMP/CPX/CPY, BIT, the shifts
and rotates, INC/DEC and the register increments, transfers, PHA/PLA, JSR/RTS/JMP, branches,
flag instructions, NOP.

Interpreter assist (one instruction through `interp_step`, state converted both ways;
costs a few hundred cycles; the trace continues afterwards): `ADC/SBC` in decimal mode, `PHP`,
every undocumented opcode (LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA, ANC, ALR, ARR, SBX and the
multi-byte NOPs), loads and read-modify-write of `$D000-$DFFF` and stores to I/O other than
the SID, stores that hit translated code, instructions on volatile bytes. Assist then dispatch
(the trace ends): `PLP`, `RTI`, `BRK`, `JMP (ind)`, illegal opcodes (which stop the run).

Stores to a SID register (`STA/STX/STY $D400-$D7FF`) are native, followed by an *event*: the
dispatcher logs `(timestamp, reg, value)` and resumes the trace (about 250-300 cycles).
Undefined opcodes and the unstable undocumented ones (XAA, AHX, TAS, SHX, SHY, LAS, `LAX #`,
KIL) stop the run with `RUN_ILLEGAL`; so does `ARR` in decimal mode.

## Verification

Everything is compared with `tools/ref6502.py`, written separately from the C code.

| Set | Tests | What |
|---|---|---|
| directed | 2,156 | every opcode and addressing mode (up to 10 random register/flag/operand/page-cross combinations each, all 236 defined opcodes) |
| control | 504 | 128 ALU runs over 256 operands each, plus branches taken/not/page-crossing, JMP/JSR/RTS/BRK/RTI/PLP/PHP/TXS, stack wrap, indirect-JMP bug, all 20 illegal opcodes, I/O reads and writes (raster, SID mirrors, `$D7FF`, dynamic stores into I/O), self-modifying code (same trace, loop, dynamic, generated code, data in the code page) |
| soups | 465 | random programs with loops, calls, forward branches, SID writes; documented only, with the undocumented set, and with stores into their own code |
| music | 2 | synthetic 3-voice sequencer, 60 frames, SID log compared |
| sweeps | 48 | every second operand pair for ADC/SBC (binary and decimal, C=0/1), CMP/CPX/CPY, logic, BIT, shifts, rotates, INC/DEC, ISC/RRA/DCP/ARR/SBX, folded into a checksum of results and flags |

Each is run on the interpreter, on the JIT, and on the JIT with chaining off and
`jit_step_hook` set: after **every trace** a second, independent copy of the interpreter
(`shadow.h`) runs up to the same 6502 cycle and the two states are compared (A X Y S PC, the
flags that are live there, cycle count, SID log length, every checked memory region). A mismatch
prints the trace's start and end pc. The final check compares registers, flags, pc, cycle
count, the whole SID log with timestamps, and the memory regions the test lists (the generator
verifies that nothing else is written).

Mutating the interpreter (decimal adjust, branch page-cross) fails the tests as it should.
Bugs the tests found while writing this: the page-crossing cycle skipped on a `CMP` whose flags are dead, a stale
decode of instructions on volatile bytes (now always interpreted and followed by a dispatch). Found by review: a link
patched after a cache flush (now guarded by a flush counter; the small-cache runs stress it).

Not covered: real hardware (only fsim); `N=1,Z=1` is representable (RES special form) but
`PLP`/`RTI` are interpreted anyway; reads from I/O through `(zp),Y`, `(zp,X)` and `zp,X` (see
limitations).

## Measured speed (fsim, `make bench`)

Synthetic player: three voices, pattern tables, vibrato, pulse and filter sweeps, plus a
table-processing loop to reach a given cost per frame (`tools/mkmusic.py`). 100 frames, native
cycles from the CPU cycle counter, 50 frames a second, 50 MHz.

| Routine | 6502 cyc/frame | core | interpreter native/6502 | interpreter % of CPU | JIT native/6502 | JIT % of CPU |
|---|---|---|---|---|---|---|
| MUSIC0 (small) | 415 | c0 | 62.8 | 2.6% | 24.6 | 1.0% |
| | | c1 | 56.3 | 2.3% | 22.7 | 0.9% |
| MUSIC2K | 2,095 | c0 | 59.2 | 12.3% | 5.7 | 1.2% |
| | | c1 | 53.7 | 11.2% | 5.2 | 1.0% |
| MUSIC5K | 5,287 | c0 | 58.6 | 30.9% | 2.9 | 1.5% |
| | | c1 | 53.2 | 28.1% | 2.6 | 1.3% |
| DIGI (160 SID writes a frame, 8 kHz) | 19,367 | c0 | 73 | 142% | 2.7 | 5.2% |
| | | c1 | 65 | 126% | 2.0 | 3.9% |

- Tight loops (`lda tab,x; eor; sta; dex; bne`) run at about **1 native cycle per 6502 cycle**.
- The small routine is dominated by SID-write events (one per register write, about 16 a frame at
  about 300 native cycles each) and by instruction-cache misses (the code runs once a frame);
  as work grows the ratio falls toward 1.
- **Translation cost**: about 400-500 native cycles per translated 6502 instruction (300 instructions
  = 120,000-150,000 cycles, 2.4-3 ms at 50 MHz, paid on the first call only). The first frame of
  MUSIC0 costs 128,000 native cycles against 10,000 for later frames.
- The interpreter (about 200 native cycles per 6502 instruction) cannot run a digi routine in real
  time; the JIT uses 4-5% of the CPU for one.

## Limitations

- Reads from the I/O area through computed zero-page or indirect addressing (`(zp),Y`, `(zp,X)`,
  `zp,X`) in JIT code return the stored byte without calling the hook; the interpreter does call it.
  Static `abs` and `abs,X/Y` (base in `$CF01-$DFFF`) are hooked in both engines.
- No interrupts: nothing raises IRQ/NMI; `BRK` jumps through `($FFFE)` but there is no CIA timer, so
  `play` must be called by the host. VIC/CIA registers are plain memory except the raster counter.
  `$D41B/$D41C` (osc3/env3) read back what was last stored.
- Code in page 1 and in `$D000-$DFFF` runs interpreted.
- The 6502's 64 KB is all RAM (no ROM/I/O banking, no `$01`). `$FFF0` is the stop address.
- `ARR` in decimal mode and the unstable undocumented opcodes stop the run.
- The cycle budget is a 16-bit down counter checked at every trace entry; traces are at most 32
  instructions, so it cannot overflow. `RUN_LIMIT` is exact only to one trace.
- NMOS only (no 65C02). Decimal-mode N/V/Z follow the NMOS behaviour.

## Next steps

- **RSID** tunes and CIA timers: a CIA1 timer A model delivering IRQs (`BRK`/`IRQ` entry through
  the interpreter at trace boundaries, with the cycle at which the timer fires as `until`), the
  `$D012` raster IRQ, and the PAL/NTSC and speed bits from the `.sid` header; `RTI` return from the
  play routine.
- **`.sid` loader** (PSID v1/v2 header parsing, `$0000` load address, song select, `play == 0`
  for tunes that install their own IRQ vector).
- **Audio engine**: `sidlog` is the interface. Drain it once a frame, convert `(cycle, reg, value)`
  to the SID model's clock; reads of `$D41B/$D41C` can be served from the model through
  `io_read_hook` (the JIT routes those loads through the interpreter already).
- **Speed**: inline SID-write logging (needs two spare registers; the event costs ~300 cycles), a
  `jit_exit`/`jit_enter` that saves fewer registers, native decimal-mode `ADC/SBC`, native `PHP/PLP`,
  and the stable undocumented opcodes (LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA) natively.
- **Hardware**: nothing here depends on fsim except `SYNC`, which the JIT executes before running
  new code. Run the same tests on the FC core RTL (`verilog/tb`) once the system has SRAM for the
  arrays above.

## Changes the rest of the tree might want

None required. Possible: `fsim` could print the guest's `printf` output when it exits on cycle limit;
`fcc` has no way to pass `-D` defines through the benchmark runner (not needed here).

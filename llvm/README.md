# llvm

The C toolchain for fpga_cpu: an LLVM 19.1.7 backend (target `fcpu`), a
clang target, a small C runtime, and the `fcc` driver.

```sh
llvm/setup.sh                       # clone LLVM, patch, build clang + llc (~25 min)
llvm/fcc prog.c -o prog.bin         # compile + link; also writes prog.sym
sim/build/fsim prog.bin             # run headless, or fsim-gui with --sym prog.sym
.venv/bin/python llvm/runtime/tests/run.py   # compiler tests: host vs target A/B
```

## How it fits together

The backend emits assembly, not object files. `sim/tools/asm.py` assembles
and links it: it picks the 16- or 32-bit encoding for every instruction
in either opcode-map variant (A or B), so one compiler serves both and the
encoding stays measurable.

| Path | Contents |
|---|---|
| `target/FCPU/` | The backend (symlinked into the checkout as `llvm/lib/Target/FCPU`) |
| `clang/` | clang target info: `int` 16 bits (32 with the `int32` target feature, `fcc --int32`); `long`, pointers and `size_t` 32 bits; `float` = `double` = IEEE single |
| `patches/` | Edits to existing LLVM files: `Triple`, clang registration, and a libc++ fix needed on the macOS 27 SDK |
| `runtime/asm/` | `crt0`, 32- and 64-bit multiply, 16/32/64-bit divide, variable 32-bit shifts, float add/subtract/multiply/divide fast paths, `__clzsi2`, `setjmp`/`longjmp` |
| `runtime/libc/`, `runtime/libm/`, `runtime/include/` | Freestanding libc subset (string, stdlib, malloc, console printf) and single-precision math |
| `runtime/tests/` | Compiler tests, each run on the host and on both variants and compared |
| `fcc` | Driver: clang → assembly → `asm.py` link with crt0 and only the runtime files needed. Other soft-float and 64-bit helpers come from compiler-rt sources. |

## fcc options

| Option | Effect |
|---|---|
| `-v A\|B` | encoding variant |
| `--int32` | 32-bit `int` (clang target feature `+int32`), linked against a runtime built the same way (`runtime/build-int32/`), for C written for 32-bit machines |
| `--base ADDR` | load address (default `0x1000`) |
| `--stack ADDR` | initial stack pointer (default `0x053400`, below the sprite buffers). The heap runs from the end of BSS to 16 KB below it. Programs above `0x0FFFFF` need `fsim --xram`. |
| `--ext[=LIST]` | link runtime routines that use the optional ISA extension X (`DSTEP`, `MULPU`, `FSHR`/`FSHL`, `CLZ`; see [cpu/isa.md](../cpu/isa.md)); `LIST` picks parts: `dstep,mulpu,fsh,clz`. Run the result with `fsim --ext`. |
| `--cflag FLAG` | pass a flag to clang for your files, e.g. `--cflag=-std=gnu89` |
| `--stats`, `--lst`, `--keep` | encoding statistics, listing, keep the `.s` files |

## Runtime library

`runtime/` is a freestanding C library: `string.h` and `strings.h`,
`stdlib.h` (malloc, qsort, strtol, strtod, ...), `ctype.h`, `math.h`
(single precision), `setjmp.h`, `errno.h`, `assert.h`, `inttypes.h`,
`time.h` (`clock()` counts CPU cycles / 16), and `stdio.h`: printf and
scanf families, and `FILE` streams. stdout and stderr are the console;
files go through the file service (docs/io.md), with flat 8.3 names (any
directory in a path is dropped), `fseek` through its `SEEK` command, and
no `rename`.

The arithmetic helpers are hand-written in `runtime/asm/`:

- **64-bit multiply** (`muldi3.s`) is built from 16×16 `MUL`/`MULHU`
  products, with no calls. Doom's `FixedMul` costs about 80 cycles.
- **64-bit divide** (`divdi3.s`) runs one or two 32-step loops when the
  divisor fits in 32 bits, sharing `__fcpu_udiv32` with `divsi3.s`.
  Wider divisors still use compiler-rt's `__udivmoddi4`.
- **Float add, subtract, multiply and divide** (`addsf3.s`, `mulsf3.s`,
  `divsf3.s`) have fast paths for normal operands, rounding to nearest
  even. Zeros, subnormals, infinities, NaNs and results that could be
  subnormal go to compiler-rt's C versions, built under the names
  `__fcpu_*_c`. `tests/fpexact.c` checks the results bit for bit.
- **Extension X:** each routine has an `.ifdef FCPU_X_<PART>` version for
  `fcc --ext` (asm.py's `-D`).

## Code generation notes

- **Registers:** i16 values live in r0–r13, and i32 values (including pointers) in pairs p0–p6; p7 is sp. p0 (r0, r1) is allocated last: the assembler uses it to expand `[abs]` loads and stores in variants without a direct form, so those instructions are marked as clobbering it, and an `[abs]` store's value never sits in it.
- **Calls:**
  - Arguments go in r2–r7 or p1–p3. Varargs go on the stack.
  - Results come back in r2, or in p1 (and p2 for 64-bit values).
  - p4–p5 are callee-saved, each saved with one STP.
  - The return address is held in a virtual register, so leaf functions never touch the stack for it.
- **32-bit operations:**
  - add: `ADDPP`/`ADDPI`
  - subtract and compare: SUB/SBC and CMP/CPC. Against 0, `==`/`!=` test the OR of the halves and `<`/`>=` the high half's sign; a `CMPI r, 0` after the instruction that computed r is dropped when its flags already say the same (`optimizeCompareInstr`).
  - logic, constant shifts and multiply: split into 16-bit halves while lowering. Halves that are already known (constants, extended 16-bit values, pairs built from halves) are looked through, so `x & 0xFC0` on a 32-bit value is one `ANDI`, and a multiply is at most four `MUL`/`MULHU` (no call).
  - divide and variable shifts: library calls
- **Addressing modes:**
  - `[pN + d16]`
  - `[pN + rX*s]` for a 16-bit index, including one masked down from a 32-bit value
  - `[pN]+` post-increment, when a pointer steps by the access size after a load or store
  - `[abs]` for globals and I/O registers
- **Scheduling:** a simple model (one instruction a cycle, loads one cycle late) for the machine scheduler before register allocation and the list scheduler after it, so independent work fills load-use stalls. Flag-only results (the OR above) are moved to a register the block doesn't use, after allocation, so they don't tie the scheduler's hands (`FCPURenameDeadDefs.cpp`).
- **Global loads in loops:** a byte store may change any object in C, so a loop that stores pixels must reload a global pointer like Doom's `dc_source` each time round. `FCPUHoistGlobalLoads.cpp` versions such loops: a check before the loop that the bytes it stores don't overlap the globals it reads chooses a copy with those loads hoisted out. (`-mllvm -fcpu-no-hoist-global-loads` turns it off.)
- **Flags:** the compiler passes `-funsigned-char`, because `LDBU` has the short encodings.

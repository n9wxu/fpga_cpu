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
| `runtime/asm/` | `crt0`, 32-bit multiply, 16/32-bit divide, variable 32-bit shifts, `setjmp`/`longjmp` |
| `runtime/libc/`, `runtime/libm/`, `runtime/include/` | Freestanding libc subset (string, stdlib, malloc, console printf) and single-precision math |
| `runtime/tests/` | Compiler tests, each run on the host and on both variants and compared |
| `fcc` | Driver: clang → assembly → `asm.py` link with crt0 and only the runtime files needed. Soft-float and 64-bit helpers come from compiler-rt sources. |

## fcc options

| Option | Effect |
|---|---|
| `-v A\|B` | encoding variant |
| `--int32` | 32-bit `int` (clang target feature `+int32`), linked against a runtime built the same way (`runtime/build-int32/`), for C written for 32-bit machines |
| `--base ADDR` | load address (default `0x1000`) |
| `--stack ADDR` | initial stack pointer (default `0x053400`, below the sprite buffers). The heap runs from the end of BSS to 16 KB below it. Programs above `0x0FFFFF` need `fsim --xram`. |
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

## Code generation notes

- **Registers:** i16 values live in r2–r13, and i32 values (including pointers) in pairs p1–p6. p0 is reserved: it is the assembler's temporary for `[abs]` in variant A. p7 is sp.
- **Calls:**
  - Arguments go in r2–r7 or p1–p3. Varargs go on the stack.
  - Results come back in r2, or in p1 (and p2 for 64-bit values).
  - p4–p5 are callee-saved, each saved with one STP.
  - The return address is held in a virtual register, so leaf functions never touch the stack for it.
- **32-bit operations:**
  - add: `ADDPP`/`ADDPI`
  - subtract and compare: SUB/SBC and CMP/CPC
  - logic: split into the two halves
  - constant shifts: split into the halves
  - multiply, divide and variable shifts: library calls
- **Addressing modes:**
  - `[pN + d16]`
  - `[pN + rX*s]` for a 16-bit `int` index
  - `[abs]` for globals and I/O registers
- **Flags:** the compiler passes `-funsigned-char`, because `LDBU` has the short encodings.

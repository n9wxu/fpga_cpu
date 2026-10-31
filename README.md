# fpga_cpu

[![CI](https://github.com/n9wxu/fpga_cpu/actions/workflows/ci.yml/badge.svg)](https://github.com/n9wxu/fpga_cpu/actions/workflows/ci.yml)

A small home computer with an 8-bit *feel*: boot to BASIC, text over
graphics, sprites, simple sound, and files on an SD card. It is not an
emulation of any existing machine.

It runs on a custom 16-bit load/store CPU designed for an LLVM backend, so
the BASIC interpreter can be written in C. The first hardware target is a
Digilent Spartan-3 Starter Board (XC3S200-FT256) plus an RP2350 expansion
board. The whole machine is simulated before anything is committed to the
FPGA.

**Try it in a browser: <https://n9wxu.github.io/fpga_cpu/>.** This is the
cycle-counting simulator compiled to WebAssembly, running the BASIC
interpreter built with the LLVM backend. Click the screen, then type
`DIR` or `RUN "SPIRO.BAS"`.

## What works

- **CPU and ISA.** 16 registers, 24-bit addresses, 16/32-bit instructions in
  two candidate encodings (A and B). Everything is generated from one table,
  [`cpu/isa.yaml`](cpu/isa.yaml): the simulator's decoder and executor, the
  assembler's rules, and the opcode-map docs.
- **Simulator** (`sim/`). Cycle-counting model with an I-cache, stream
  buffer, video DMA, stall and fusion statistics, and all the devices of
  [`docs/io.md`](docs/io.md): 640×360 video with text, bitmap and sprite
  layers, keyboard, 3-voice sound, files, and the board's LEDs and switches.
  `fsim` runs headless; `fsim-gui` adds registers, disassembly, breakpoints,
  stepping, memory, console and statistics windows. The web page has the
  same windows under its Debug button.
- **Assembler and linker** (`sim/tools/asm.py`). Multi-file, sections,
  library archives, symbol files, and the shortest encoding for every
  instruction.
- **LLVM backend** (`llvm/`). clang and llc for LLVM 19.1.7: `int` is 16
  bits, `long` and pointers 32. The `fcc` driver compiles and links C, with
  a small libc, libm, and compiler-rt's soft-float.
- **BASIC** (`basic/`). An MS-BASIC-style interpreter in C with graphics,
  sprites, sound and files. It gives the same test output on the host and
  in the simulator on both encodings.
- **Web build** (`web/`). The simulator in a browser running BASIC, with
  a disk kept in the browser's storage, or Doom
  ([play it](https://n9wxu.github.io/fpga_cpu/?prog=doom)).
- **T1 core** (`verilog/`). The smallest core of the ladder
  ([docs/t1.md](docs/t1.md)): the whole ISA on an 8-bit datapath, in
  Verilog-2001, checked instruction by instruction against the simulator.
  It is simulated in Icarus; it has not been through the ISE build yet.

Not started yet: the ISE build (`fpga/`) and the expansion board
(`pcb/`). The open design questions are in the
[issues](https://github.com/n9wxu/fpga_cpu/issues).

## Quick start

macOS or Linux with a C compiler, `make` and Python 3. The first `make`
creates a Python virtual environment in `.venv` for the tools.

```sh
make -C sim            # build/fsim (and build/fsim-gui if SDL2 is installed)
make -C sim test       # ISA self-test, benchmarks, device tests
make -C basic test     # BASIC on the host
```

The compiler needs CMake and Ninja, and takes about 25 minutes to build:

```sh
llvm/setup.sh                    # LLVM 19.1.7 + the FCPU backend -> llvm/build
.venv/bin/python llvm/runtime/tests/run.py   # compiler tests: host vs both encodings
llvm/fcc hello.c -o hello.bin    # compile and link a C program
sim/build/fsim hello.bin         # run it
```

Benchmarks for ISA work (Dhrystone, Whetstone, Stanford, CoreMark, BASIC,
Doom), compiled, run in the simulator, checked and timed:

```sh
benchmarks/run.py --quick        # every benchmark, base timing model
benchmarks/doom/get_wad.sh && benchmarks/run.py doom
```

BASIC on the simulated machine:

```sh
make -C basic target             # basic/build/basic.{A,B}.bin
make -C basic test-target        # the BASIC tests in the simulator
sim/build/fsim-gui basic/build/basic.A.bin --sym basic/build/basic.A.sym --disk basic/examples
```

The browser version needs Emscripten (`emcc`):

```sh
make -C web serve                # http://localhost:8000
```

## Layout

| Directory  | Contents |
|------------|----------|
| `cpu/`     | ISA specification and `isa.yaml`, the single source of truth |
| `sim/`     | Simulator, GUI, assembler/linker, benchmarks and measurement tools |
| `llvm/`    | LLVM backend, clang target, `fcc` driver, C runtime and tests |
| `basic/`   | BASIC interpreter in C, tests and example programs |
| `benchmarks/` | Classic benchmarks (Dhrystone, Whetstone, Stanford, CoreMark, ...), BASIC benchmarks and Doom, for ISA work |
| `web/`     | The simulator compiled to WebAssembly |
| `docs/`    | Architecture, I/O map, measurements |
| `verilog/` | T1 core RTL, testbenches with lockstep comparison against the simulator, transistor count ([verilog/README.md](verilog/README.md)) |
| `fpga/`    | ISE 14.7 build scripts and board constraints (to come) |
| `pcb/`     | Expansion board: RP2350, SD, sound, I/O (to come) |

Start with [docs/architecture.md](docs/architecture.md) for the design and
[docs/measurements.md](docs/measurements.md) for the numbers behind it.

## Workflow

1. Change the ISA in `cpu/isa.yaml`; the generators rebuild the simulator
   tables and the assembler rules.
2. Measure in the simulator: instruction mix, I-cache behavior, stalls,
   fusion candidates.
3. Keep the LLVM backend and BASIC building and passing their tests.
4. Write RTL and run it in lockstep against the simulator under Verilator.
5. Build with ISE 14.7 on a Linux host and program the board with
   openFPGALoader.

CI (`.github/workflows/ci.yml`) builds and tests all of this on every push
and pull request, and publishes the web build to GitHub Pages from `main`.

## License

MIT. See [LICENSE](LICENSE). The Terminus font in `sim/assets` is under
the SIL Open Font License, and Dear ImGui in `sim/third_party` under MIT.

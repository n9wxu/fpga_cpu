# sim

Simulator, assembler and measurement tools for the fpga_cpu ISA. Everything is
driven by [`../cpu/isa.yaml`](../cpu/isa.yaml).

```sh
make -C sim            # generate tables from isa.yaml, build sim/build/fsim (+ fsim-gui if SDL2 is installed)
make -C sim gui        # build sim/build/fsim-gui only
make -C sim test       # ISA self-test, every benchmark on every variant, device test
make -C sim measure    # full sweep -> docs/measurements.md
```

The Makefile creates `.venv/` (PyYAML) on first use. The GUI needs SDL2
(`brew install sdl2`); Dear ImGui is vendored in `third_party/imgui`.

## Layout

| Path | Contents |
|---|---|
| `tools/isa.py` | Loads `isa.yaml`, expands it into individual encodings, checks for overlaps and length-rule violations. Running `isa.py --selftest` does random encode/decode round trips. |
| `tools/gen.py` | Writes the C tables (`src/gen/`, not committed) and the encoding docs (`cpu/gen/encoding_*.md`) |
| `tools/asm.py` | Assembler: picks the shortest encoding that fits, settles branch sizes over repeated passes, writes a listing and per-encoding statistics |
| `tools/measure.py` | Runs every benchmark × variant × timing configuration and checks each output |
| `tools/bench_ref.py` | Python reference results for the benchmarks (`--update` rewrites the `; expect:` lines) |
| `tools/devtest.py` | Device test: runs `demo/devtest.s` headless and checks the keyboard queue, file service and board I/O |
| `tools/mkfont.py` | Builds the 8×14 character ROM (`assets/font8x14.mem` for the FPGA, `src/font8x14.c` for the simulator) from the Terminus BDF |
| `src/cpu.c` | CPU core: decode, memory map, timing model, `cpu_step`/`cpu_run` |
| `src/dev.c` | I/O devices from [docs/io.md](../docs/io.md): console, frame timing, keyboard, video registers, sound, file service, board I/O |
| `src/video.c` | Video renderer: backdrop, bitmap (8-color / hi-color), sprite buffer, text; 640×360 doubled to 1280×720 |
| `src/front.c` | Shared by both front ends: options, image loading, symbols |
| `src/main.c` | Headless front end, `fsim` |
| `src/gui.cpp` | SDL2 + Dear ImGui front end, `fsim-gui` |
| `bench/` | Benchmarks plus `lib.s` (console output, decimal/hex printing, exit) |
| `demo/` | `video.s` (every display layer, double-buffered sprite), `hicolor.s` (palette and dither), `devtest.s` |
| `assets/` | Character ROM: `ter-u14n.bdf` (Terminus, SIL OFL 1.1, see `OFL.TXT`) and the generated `font8x14.mem` |

## Running a program

```sh
.venv/bin/python sim/tools/asm.py -v A prog.s -o prog.bin --lst prog.lst
sim/build/fsim -v A prog.bin              # --trace, --json, timing options: fsim -h
```

Programs start at `.org 0x1000` by default (`--load`, `--entry`). Both
front ends take the same options (`fsim -h`):

| Option | Effect |
|---|---|
| `-v A\|B` | encoding variant |
| `--disk DIR` | host directory served by the file service (default `./disk`) |
| `--sym FILE` | symbols, lines of `%06x name`; used by the GUI's disassembly |
| `--keys STRING` | preload the keyboard queue (`\n` = Enter, `\e` = Esc) |
| `--screenshot FILE` | write the 1280×720 display as PPM at exit |
| `--true-color` | show hi-color palette colors instead of the 3-bit dither |
| `--trace`, `--json FILE`, `-q` | headless only: instruction trace, statistics, no summary |
| `--dump A:N:FILE` | headless only: write N bytes of memory from A at exit |

The I/O registers and the memory layout are in [docs/io.md](../docs/io.md).
Headless, `CON_OUT` goes to stdout and `CON_IN` reads stdin.

## GUI

```sh
sim/build/fsim-gui sim/build/video.bin --sym video.sym
```

| Window | Contents |
|---|---|
| Display | the 1280×720 output. Click it to type: keys go to the keyboard queue as the io.md key codes |
| Controls | Run (F5), Stop, Step (F11), Step Over (F10, steps over `JAL`/`JALR`), Step Out (runs until a `JR` returns from the current frame), Reset (reloads the image), speed: real time 50 MHz or as fast as possible |
| Registers | r0–r15 (editable while stopped), the pairs with symbol names, PC, NZCV, cycles, frame and scan line |
| Disassembly | follows the PC, shows labels from `--sym`; click a line to toggle a breakpoint; go to an address or symbol |
| Memory | hex and ASCII over the whole 24-bit space; go to an address or symbol |
| Serial console | `CON_OUT` text; a line typed here is sent to `CON_IN` with `\n` |
| Board | the starter board's 7-segment display, LEDs, slide switches and push buttons |
| Statistics | IPC, I-cache, branches, stall breakdown, fusion candidates |

Sound plays the three voices through SDL audio. The window layout is
saved in `~/.fsim_gui.ini`.

Scripted runs, for checking the GUI without clicking:
`--paused` starts stopped; `--frames N --screenshot-window FILE.bmp` runs N
display frames, saves the whole window and exits; `FSIM_GUI_SELFTEST=1`
checks breakpoints, Step Over/Out and key translation on `demo/video.s`.

## Assembly syntax

```asm
        ADD   r1, r2, r3        ; three-operand
        ADD   r1, r2            ; r1 = r1 + r2 (assembler picks the 16-bit form)
        LDW   r1, [p2 + 6]      ; pair + displacement
        LDW   r1, [p2 + r3*2]   ; pair + sign-extended index (r3.z*2 = zero-extended)
        LDW   r1, [p2]+         ; post-increment     STW r1, -[p2]   pre-decrement
        LDW   r1, [label]       ; direct (variant A expands to LEA p0 + LDW)
        BNE   loop              ; EQ NE CS/HS CC/LO MI PL VS VC HI LS GE LT GT LE AL/RA
```

- Registers are `r0`–`r15`. Pairs are `p0`–`p7`, with `sp` = `p7` and `lr` = `p6`.
- Pseudo-instructions: `LI LA CALL RET PUSH POP PUSHP POPP INC DEC CLR TST`.
- Directives: `.org .equ .byte .word .long .ascii .asciz .align .space .include`.

## Timing model

CPU and SRAM share one 50 MHz clock and one 32-bit memory port.

- **Every instruction takes 1 cycle**, plus:
  - I-cache misses: 1 cycle to start, then 1 cycle per word, word by word.
  - The stream buffer prefetches the next line during idle port cycles.
  - Taken branches: 2 cycles.
  - A load whose result the next instruction uses: 1 cycle.
  - Video scan-out takes port cycles: 4 every 56 by default.
  - Each I/O access: 2 cycles.
  - A 32-bit pair access that spans two SRAM words: 1 cycle.
- **Fusion candidates are always counted.** `--fuse` applies them: the second instruction issues free when both instructions fit in the 64-bit fetch window.

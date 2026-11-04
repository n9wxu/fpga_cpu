# System architecture

Status: draft. It records the decisions made so far. Items marked
**open** are still to be decided, mostly by measuring in the simulator.

## Goals

- Boot to BASIC with an 8-bit *feel*. It is not an emulation of any
  existing machine.
- Text over graphics, sprites, simple sound, files on SD, and GPIO/I2C/SPI
  for sensor experiments.
- Simulate everything on the Mac first.

## Hardware target (base machine)

Digilent Spartan-3 Starter Board:

- XC3S200-FT256: 3,840 LUT4s, 12 block RAMs, 12 hardware multipliers
- 1 MB of 10 ns async SRAM: two ×16 chips that share address lines, used
  as one 32-bit memory
- 3-bit VGA (8 colors), PS/2, RS-232
- 50 MHz oscillator

RP2350 expansion board, on headers A2/B1:

- FAT filesystem on SD, keyboard, simple synth sound (PWM), RTC
- Link to the FPGA: 2–3 Mbaud UART plus an IRQ line

Future expansion, **not designed yet**: better video (VRAM, HDMI), sound,
storage and game-controller input, and a sprite engine with its own
sprite RAM (see Video).

## Later hardware targets

The Spartan-3 board plus one expansion PCB is the first build: a single,
sturdy assembly. A larger board comes after it, probably a Digilent Arty
A7-100T (XC7A100T, DDR3, no video or PS/2) or a Tang Nano 9K (GW1NR-9,
8 MB PSRAM, onboard HDMI). Either has room for issues #6 (hardware sprites)
and #2 (dual issue), but needs its own memory and video design. Keep the CPU
RTL vendor-neutral, with memory, video and pins in a per-board layer.

## CPU

- 16-bit datapath, load/store design.
- 16 × 16-bit GPRs. Even/odd pairs form 8 × 24-bit pointer registers.
- 24-bit address space.
- NZCV flags, with ADC/SBC/CPC for 32-bit arithmetic.
- Variable-length instructions, 16 or 32 bits, aligned to 2 bytes. The
  first 16-bit parcel's top bits give the length.
  - Fetch reads a 64-bit window from an I-cache banked into even and odd
    words, so every instruction takes one fetch.
  - **Open:** how much of the opcode map goes to 32-bit instructions:
    - Split A: 3/16 32-bit.
    - Split B: MSB=0 means 32-bit.

    Both will be measured.
- Addressing modes:
  - `[pn+d16]`
  - `[pn+rX×1/2/4]` with sign or zero extension
  - `[pn]+` and `-[pn]`
  - direct `[abs24]`, through an address load into a pair
  - `LDP`/`STP` move a whole pair in one access
- Optional macro-op fusion: CMP+Bcc, ADD+ADC, address load + LD. Add only
  if the simulator shows it pays.
- Memory system:
  - 2 KB direct-mapped I-cache with 16-byte lines, plus a one-line
    stream prefetch buffer. Fills stall the CPU.
  - Prefetch runs only when the memory port is idle.
  - No D-cache.
  - 4-stage pipeline.

### Software conventions (LLVM)

- `int` = 16 bits. `long`, pointers and `size_t` = 32 bits.
- DataLayout `e-m:e-p:32:16:16:32-i32:16-i64:16-n16-S16`.
- Instruction selection folds a sign- or zero-extended 16-bit index into
  the `[pn+rX]` mode.
- 16-bit forms come from TableGen `CompressPat`, the same way RISC-V
  compressed instructions work.

## Memory map

| Range           | Use |
|-----------------|-----|
| 000000–0FFFFF   | 1 MB SRAM, 32-bit, holding code and data |
| 100000–EFFFFF   | Reserved for future expansion |
| F00000–F0FFFF   | I/O |
| FF0000–FFFFFF   | Boot ROM (block RAM), loader only |

BASIC loads from SD into SRAM at boot. Pointers can read tables stored in
code. `SYNC` invalidates the I-cache after code has been written.

The C runtime places the video buffers at the top of SRAM (details in
[io.md](io.md)): the sprite buffers at 053400, the bitmap at 0C3C00 and
the text at 0FC000. That leaves 000000–0533FF (333 KB) for code, data,
heap and stack. Video takes the other 691 KB.

## Video

- Output: 1280×720 @ 60 Hz on the VGA connector, with the standard
  74.25 MHz pixel clock from two DCMs in series (50 × 27/20 × 11/10).
  One DCM's 75 MHz (×3/2) is 1% fast, and a monitor tested with it
  (Samsung 2343BWX) placed the picture badly; `fpga/rtl/video720.v` has
  the timing and the text layer.
- Logical screen: 640×360, shown at 2× (each pixel and line doubled).
- Layers, back to front:
  1. Backdrop: a solid color. This is the default.
  2. Background bitmap, with hardware scroll. It has two modes:
     - 8-color: a 4-bit `[T][R][G][B]` nibble per pixel.
     - Hi-color: 8-bit indices into a 256 × 18-bit palette.
     - On the base hardware, hi-color is shown on the 3-bit VGA through
       a 2×2 ordered dither, giving about 125 apparent colors.
  3. Sprite layer: double-buffered, drawn by the blitter (by software
     for now), not by hardware sprite logic. It has two modes:
     - 4 bpp, transparent wherever the T bit is set (8 colors).
     - 8 bpp, through the same palette as the hi-color bitmap, with index
       0 transparent. This is what BASIC uses, so sprite sheets keep
       their colors. Two buffers take 460,800 bytes of SRAM.
  4. Text: 80×25 with an 8×14 font. Each cell has a foreground color, a
     background color or transparency, and a stipple bit.
- Colors 0–7 are the same 8 primaries in every mode and on future
  hardware.
- Drawing engine: plot, line, rectangle, fill and read, with clipping
  (viewports), 8×8 fill patterns and transparent copy (sprites).
- Text windows need no extra hardware: they are regions of cells with an
  opaque background.
- **Scan-out cost of the 8 bpp sprite layer.** It doubles the sprite
  layer's SRAM reads to 160 words per logical line (44.4 µs, 2,222 CPU
  cycles). With a hi-color bitmap, sprites and text, scan-out takes 360
  words per line, 16.2% of the port, up from 280 (12.6%). On the
  benchmarks that costs 0.5% more CPU cycles if scan-out reads are
  spread through the line (`--video 25:4` against `32:4`), or 4% if
  each line is read in one burst (`2222:360` against `2222:280`).
- **Sprites today, and a sprite engine later.** BASIC already splits
  sprites the way a hardware engine would:
  - *Sheets* are images decoded once into memory as palette indices: the
    contents a sprite RAM would hold. Once loaded they are rarely
    rewritten.
  - *Sprite attributes* are a table of 16 entries: sheet and frame, hot
    spot, position, velocity, rotation, spin, scale, flip and animation.
    `VSYNC` steps velocity, spin and animation once per frame.
  - *Drawing* walks each sprite's screen box and maps every pixel back
    into its frame through a 2×2 inverse transform (16.16 fixed point,
    two adds per pixel along a row). This is the loop an affine sprite
    engine would run in hardware, line by line, from a banked sprite RAM,
    with no frame buffer for sprites at all.
  - *Collision* (`HIT`) samples both sprites through the same mapping,
    so it stays pixel-exact under rotation and scaling. Hardware could
    flag overlaps of non-transparent pixels as it composites.

  In software, a rotated or scaled sprite costs time in proportion to
  the screen area it covers, roughly a dozen instructions per pixel by
  inspection (not yet measured on the target). A 64×64 sprite turned 45°
  covers about 8,000 pixels, a few milliseconds of a 16.7 ms frame, so
  only a handful of large rotated sprites fit in one frame. That is the
  case for the hardware engine. A banked sprite RAM (external, since 12
  block RAMs hold only 24 KB) would also give the 460 KB of sprite
  buffers back to programs.
- **Palette.** Bitmap and sprites both look up the one 256-entry palette.
  Each logical pixel lasts two pixel clocks, so one block-RAM port can
  serve both lookups by alternating, with the other port left for CPU
  writes. No extra block RAM is needed, but this is unverified until the
  RTL exists.

## Budget (estimates)

About 3.0–3.45k of 3,840 LUTs and 11 of 12 block RAMs. This is close to
the limit, so if place and route gets tight, the optional fusion logic
goes first.

The CPU scales through a ladder of core configurations (c0–c3), each a
set of simulator, compiler and later RTL parameters, ordered by speed per
LUT: see [cores.md](cores.md). The longer plan, one 32-bit ISA at
transistor budgets from a 6502's up, compared with the CPUs of each
generation, is [scaling.md](scaling.md).

## Toolchain

- **Simulator:** written from scratch in C, table-driven from `cpu/isa.yaml`.
  - Headless `fsim`, for tests and measurements.
  - `fsim-gui` (SDL2 + Dear ImGui) with the display, registers, disassembly with symbols, breakpoints, step/over/out, memory, serial console, board I/O and statistics.
  - Devices per [io.md](io.md).
  - CPU and SRAM share one 50 MHz clock.
- **Assembler/linker:** `sim/tools/asm.py`.
  - Picks the shortest encoding for each instruction, in either opcode-map variant.
  - Links several files (local symbols, sections, archive-style libraries) and writes `.sym` files for the GUI.
- **C compiler:** LLVM 19 backend plus `llvm/fcc`. See [llvm/README.md](../llvm/README.md).
- **BASIC:** `basic/`, written in C. It runs on the host and in the simulator with identical test output.
- **Measurement:** `make -C sim measure` runs every benchmark × variant × timing configuration and checks each output. Results are in [measurements.md](measurements.md).
- **Not built yet:**
  - RTL simulation (Icarus/Verilator in lockstep against the simulator's trace)
  - The sprite layer, keyboard, sound, files and serial console in the FPGA build (T1, SRAM, video and the board I/O run; see `fpga/README.md`)

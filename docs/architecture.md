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
storage and game-controller input.

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

## Video

- Output: 1280×720 @ 60 Hz on the VGA connector, with a 75 MHz pixel
  clock from the DCM.
- Logical screen: 640×360, shown at 2× (each pixel and line doubled).
- Layers, back to front:
  1. Backdrop: a solid color. This is the default.
  2. Background bitmap, with hardware scroll. It has two modes:
     - 8-color: a 4-bit `[T][R][G][B]` nibble per pixel.
     - Hi-color: 8-bit indices into a 256 × 18-bit palette.
     - On the base hardware, hi-color is shown on the 3-bit VGA through
       a 2×2 ordered dither, giving about 125 apparent colors.
  3. Sprite layer: 4 bpp, double-buffered, transparent wherever the T bit
     is set. Sprites are drawn by the blitter, not by hardware sprite
     logic.
  4. Text: 80×25 with an 8×14 font. Each cell has a foreground color, a
     background color or transparency, and a stipple bit.
- Colors 0–7 are the same 8 primaries in every mode and on future
  hardware.
- Drawing engine: plot, line, rectangle, fill and read, with clipping
  (viewports), 8×8 fill patterns and transparent copy (sprites).
- Text windows need no extra hardware: they are regions of cells with an
  opaque background.

## Budget (estimates)

About 3.0–3.45k of 3,840 LUTs and 11 of 12 block RAMs. This is close to
the limit, so if place and route gets tight, the optional fusion logic
goes first.

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
  - The FPGA build: ISE 14.7 on the Linux host, programmed from the Mac with openFPGALoader

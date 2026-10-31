# doomframe: a Doom frame in 64 KB

Doom's renderer, cut down until a whole frame's work fits a 16-bit
address space without banking: code, tables, map, textures, working
arrays, stack and the frame buffer. It measures what one Doom frame
costs on machines that can't run Doom, and the same program runs
unchanged on the tiers that can.

Each frame does what `R_RenderPlayerView` does:

- **BSP:** walk the tree front to back (`R_RenderBSPNode`, `R_CheckBBox`).
- **Walls:** clip and project them (`R_AddLine`,
  `R_ClipSolidWallSegment`, `R_StoreWallRange`), then draw them a column
  at a time (`R_RenderSegLoop`, `R_DrawColumn`).
- **Floors and ceilings:** collect visplanes and draw them as spans
  (`R_FindPlane`, `R_CheckPlane`, `R_MakeSpans`, `R_MapPlane`, `R_DrawSpan`).
- **Sprites:** project them and clip them against the walls
  (`R_ProjectSprite`, `R_DrawSprite`, `R_DrawMaskedColumn`).

All of it uses Doom's 16.16 fixed point, including the 64-bit products
and quotients in `FixedMul` and `FixedDiv`. The level has a hall with
two pillars, a raised platform, a sunken side room behind a low doorway,
a bright skylight and three things. Eight views of it are rendered,
one per frame.

## Files

| File | |
|---|---|
| `doomframe.c` | the renderer and `main` |
| `gen_data.py` | makes `doomframe_data.h`: the map and its BSP tree (a small node builder), textures, palette, colormaps, the trig tables and Doom's start-up tables (`viewangletox`, `yslope`, light levels), with Doom's integer arithmetic |
| `doomframe_data.h` | generated, checked in |

## The rules

- **The frame is drawn into video memory.** That is a fixed region the
  display reads, `VRAM_ADDR` (128×80, 8 bits per pixel, 10,240 bytes).
  Tiers with a data cache must not keep the frame in it: video memory
  is uncacheable or write-through there, so every pixel write reaches
  it. A benchmark whose frame stays in a cache isn't a graphics
  benchmark.
- **It prints nothing.** After each frame it stores the frame number in
  `frame_done` (0 at the end of start-up). The simulator logs every
  instruction and data access (`fsim --log`). `sim/tools/memlog.py`
  splits the log at those stores. It rebuilds each frame from the
  writes to video memory, compares it with the reference build's frames,
  and reports cycles, instructions and bytes moved per frame.
- **Nothing is set up at run time.** Every table comes from
  `gen_data.py`, so start-up is a few thousand cycles: clearing the
  frame and filling a few pointer tables.

## Memory map (fpga_cpu build)

| Range | Use |
|---|---|
| 0000–00FF | unused |
| 0100–D033 | code 19.1 KB, tables, map and data 23.5 KB, working arrays 10.4 KB |
| D034–D6FF | stack, 1.7 KB from D700 down (the frames use 500 bytes) |
| D700–FEFF | video memory, 10,240 bytes |
| FF00–FFFF | free for I/O and vectors on a 16-bit machine |

## Running it

```sh
cd /tmp/df && C=~/fpga_cpu/benchmarks/c/doomframe
cc -O2 -DREFERENCE -o ref $C/doomframe.c && mkdir -p ref.d && (cd ref.d && ../ref)
~/fpga_cpu/llvm/fcc -O2 --base 0x100 --stack 0xD700 -D VRAM_ADDR=0xD700 -o df.bin $C/doomframe.c
~/fpga_cpu/sim/build/fsim --load 0x100 --log df.log df.bin
~/fpga_cpu/sim/tools/memlog.py df.log --sym df.sym --marker frame_done --vram 0xD700:128:80 --ref ref.d
```

The reference build writes `frameN.raw` (the pixels) and `frameN.ppm`
(to look at). Edit `gen_data.py`, not the header, and regenerate with
`./gen_data.py > doomframe_data.h`.

## What was cut from Doom, and why

| Doom | Here | Saves |
|---|---|---|
| 320×200 frame | 128×80, the same aspect | 54 KB of frame buffer; per-column arrays |
| 8192 fine angles, 2048-entry slope table | 512 and 256 | 61 KB of trig tables |
| 32 colormaps, pointer tables for light | 16, colormap numbers | 4 KB, plus the pointer tables |
| 128-tall wall textures, 64×64 flats | 32×64 walls, 32×32 flats (span loop adjusted) | most of the texture memory |
| 128 visplanes, 256 drawsegs, `SCREENWIDTH * 64` openings | 16, 32, `SCREENWIDTH * 2` | working memory (the views use 16, 19 and 55) |
| sky, masked mid-textures, fuzz, translation, the player's weapon, status bar | left out | code |
| sprite rotations from 8 patches | one patch, mirrored for rotations 5–7 | sprite memory; the rotation is still computed |
| ±0x7fffffff clip sentinels | ±0x7fff | lets `int` be 16 bits |

Doom's `int` variables that hold fixed-point values are `fixed_t`
here, and the `int` ones fit 16 bits. That makes the program correct
with a 16-bit `int` (fcc's default, llvm-mos, SDCC) as well as a
32-bit one, and the frames are the same with either.

## Results: tier 1 and tier 2 (per frame, mean of 8)

| CPU | How | Cycles per frame | vs Z80 per clock | Frames/s at 4 MHz |
|---|---|---|---|---|
| T1 RTL: today's 16-bit ISA, 8-bit ALU and bus | measured, Icarus, frames checked | 2,295,200 (CPI 4.86) | 18.3× | 1.74 |
| T1 RTL, `-DFIXED32` | measured, Icarus, frames checked | 1,988,872 | 21.2× | 2.01 |
| T1, `scaling.md` model (32-bit ISA) | ARM-proxy mix priced at T1 | 3,718,000 | 11.3× | 1.08 |
| T2, `scaling.md` model (16-bit ALU and bus) | ARM-proxy mix priced at T2 | 1,196,300 | 35× | 3.3 |
| Z80, SDCC 4.5, `-DFIXED32` | measured, ucsim, frames checked | 42,074,220 | 1.0× | 0.095 |
| Z80, SDCC 4.5, Doom's 64-bit `FixedMul` | measured, ucsim, frames checked | 634,954,740 | 0.07× | 0.006 |
| 6502, 65C02 (llvm-mos) | needs 76–79 KB | doesn't fit | | |
| fpga_cpu c0 FPGA core, for scale | measured, fsim log | 620,760 | | |

The Z80 builds are size-optimized (`--opt-code-size`) to fit, with video
memory at D600 and the simulator port at 00F0. `-DFIXED32` replaces the
64-bit `FixedMul`/`FixedDiv` with exact 32-bit versions: SDCC's
`long long` runtime makes the 64-bit ones 15× slower on the Z80, while on
fpga_cpu the two differ by 2%.

**Multiply's share** (`-DFIXED32` on both): the Z80 spends 41.9% of a
frame in SDCC's `__mullong` (40,555 calls in 8 frames, about 3,475
T-states each, measured with ucsim breakpoints and a microbenchmark); T1
spends 14.6% in `MUL`/`MULHU`. Without multiply T1 is 14.4× the Z80 per
clock; multiply takes it to 21.2×. The Z80 makes 4.73 T-states per
memory access (ucsim's counts: 71.2 M accesses in 337 M T-states), so at
equal memory speed T1 is about 4.5× the Z80.

On T1 an instruction moves 3.2 bytes (1.51 MB per frame: 84% instruction
fetch), so its 8-bit bus is busy 66% of cycles.

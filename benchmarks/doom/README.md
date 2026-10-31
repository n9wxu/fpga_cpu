# Doom

Doom, through [doomgeneric](https://github.com/ozkl/doomgeneric) (GPL v2,
see `LICENSE`; upstream commit in `UPSTREAM`), as a large real program for
ISA work: about 80 C files of game logic, BSP traversal, fixed-point math
and column/span rasterizing.

## What it needs

- **A 32-bit `int`.** Doom is written for 32-bit machines: `fixed_t` is
  `int` and `FRACUNIT` is `1 << 16`. It is built with `fcc --int32`.
- **Several megabytes of RAM.** The zone heap alone is 6 MB, more than the
  base board's 1 MB SRAM. It runs from the simulator's expansion RAM
  (`fsim --xram 14`), linked at `0x100000` with the stack at `0xF00000`.
  Only video stays in the SRAM. So this is a benchmark for the simulator,
  not a program for the base board.
- **An IWAD.** `./get_wad.sh` uses `wad/DOOM1.WAD` if you put the shareware
  v1.9 file there (its `demo1` is the classic timedemo), and otherwise
  fetches Freedoom: Phase 1 (BSD) and installs it as `wad/DOOM.WAD`. The
  WAD is read lump by lump through the file service, which gained `SEEK`
  and `SIZE` for this (docs/io.md), so it doesn't have to fit in memory.

## The port (`port/`)

- `doomgeneric_fcpu.c`: video, keyboard, time and `main`.
  - doomgeneric's 8-bit mode with a 640×360 frame buffer, which is the
    hi-color bitmap's own layout, so the picture is copied straight into
    video memory (centered) and Doom's palette goes into the hardware
    palette. The picture is the full 640×350 screen, or the original
    320×200 (see "Full screen" below).
    Palette index 0 is transparent on this machine, so the backdrop is
    set to black.
  - Keys: the port turns on key releases (`KBD_CTRL`, docs/io.md), so a
    held key stays held; Ctrl (fire), Shift (run) and Alt (strafe) follow
    the live modifier state.
  - Time is the CPU cycle counter at 50 MHz.
  - Arguments come from `DOOMARGS.TXT` on the disk, if it exists.
- `r_draw_fcpu.s`: `R_DrawColumn` and `R_DrawSpan` by hand, and their
  low-detail versions `R_DrawColumnLow` and `R_DrawSpanLow` (built with
  `-DFCPU_DRAW_ASM`, which leaves out the C ones in `src/r_draw.c`). The
  Makefile and the web simulator use them; `make DRAW=c` builds the C.
  `../run.py` measures both: `doom` (C) and `doom-asm`.
- `doomgeneric_host.c`: the same engine headless on the development
  machine, the reference for the simulator run.
- `include/`: the few POSIX headers the engine includes. Everything else
  comes from the C runtime, which gained buffered-free file streams,
  `scanf`, `errno` and more for this (llvm/runtime).

## Changes to the engine (`src/`)

All marked `fpga_cpu:` in the source:

- `g_game.c`: `-timedemotics N` ends a timedemo after N gametics, so a
  slow (simulated) machine can be timed on part of a demo.
- `r_draw.c`: `FCPU_DRAW_ASM` leaves out `R_DrawColumn` and
  `R_DrawSpan` for the hand-written ones.
- `d_main.c`: no screen melt during a timedemo. The melt runs for about a
  second of real time on any machine, so leaving it out keeps a short
  timedemo a measure of game frames. And two arguments, because
  doomgeneric does not read the configuration file: `-lowdetail` (the
  menu's "detail: low") and `-screenblocks N` (the view size, 3–11).
- Full screen (`-DDOOM_HIRES`), below.

## Full screen

`-DDOOM_HIRES` renders at 640×350 instead of 320×200 (`src/i_video.h`);
the Makefile builds it by default, `make RES=320` builds the original.

- The 3D view keeps the original framing, stretched 2× wide and 1.75×
  tall like everything else: the vertical projection (`projectiony`,
  `r_main.c`) is 7/8 of the horizontal one, and sprites and the sky use
  it too. Wall light levels use a longer table (`MAXLIGHTSCALE` 84), so
  the lighting matches 320×200.
- The menus, status bar, HUD, intermission, finale and automap marks
  still place their graphics in 320×200 coordinates; `v_video.c` scales
  each patch pixel to a 2×1.75 block (`SCALEX`, `SCALEY`). The view
  border is drawn in screen pixels.
- Visplanes keep 16-bit row numbers (`visrow_t`, `r_defs.h`).
- `-DDOOM_DIRECT` draws straight into video memory instead of copying
  the finished frame (saves 0.1–0.75 M cycles a frame, but the picture can
  tear: there is no second page).

Without `DOOM_HIRES` the picture is the same as before, byte for byte.
`../run.py doom640` and `doom640-asm` check the full-screen build the
same way as `doom`: the whole 640×350 last frame must match the host's.

Speed at full screen, `-timedemo demo1`, 100 gametics, variant A, with
the hand-written draw loops (fps at 50 MHz):

| | c0 | c0 + ext X | c3 | c3 + ext X |
|---|---|---|---|---|
| full detail | 4.13 | 4.62 | 5.85 | 6.25 |
| full detail, `DOOM_DIRECT` | 4.43 | 5.04 | 6.29 | 6.65 |
| `-lowdetail` | 6.33 | 7.04 | 9.46 | 10.57 |
| `-lowdetail`, `DOOM_DIRECT` | 7.16 | 8.14 | 10.67 | 12.15 |

For comparison, 320×200 is 8.27 fps on c0.

## Playing it

In the browser: the web simulator (`web/`, published at
https://n9wxu.github.io/fpga_cpu/?prog=doom) has Doom under Program.
Locally: `make` here builds `build/doom.A.bin`, then

```sh
./get_wad.sh
mkdir -p build/disk && cp wad/DOOM.WAD build/disk/
../../sim/build/fsim-gui --xram 14 --load 0x100000 --disk build/disk --true-color build/doom.A.bin
```

## Running the benchmark

```sh
./get_wad.sh
../run.py doom                 # build, run 100 gametics of demo1, check, report
```

The check: the simulator must play the same number of gametics as the
host build and end on exactly the same frame (palette indices compared
byte for byte), which exercises the compiler, the runtime and the ISA
model together.

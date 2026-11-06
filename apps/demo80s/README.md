# demo80s: the 80s home computers, then the Tang pulling ahead

A demo for the Tang Nano 20K in four parts, about two and a half minutes, looping. Everything is original:
the graphics are drawn at run time and the music is composed and synthesized by `tools/mkmusic.py`.

| Part | What it shows | Sound |
|---|---|---|
| Apple II | a 40-column screen with an Applesoft-style program typed in, then 280x180 hi-res in the six artifact colours: moire line fans and a bouncing ball, all CPU drawing, mixed-mode text | the one-bit speaker (a square voice) and key clicks |
| Commodore 64 | a BASIC start-up screen (a parody banner), `LOAD` and tape-loading stripes in the border, then raster bars, a colour-cycled block logo, eight hardware-sprite UFOs and a smooth scroller | an original three-voice SID tune: PWM bass through the filter, 50 Hz chord arpeggios, a lead with vibrato, drums stolen from the voices |
| Amiga | a copper-gradient sunset on playfield B with a perspective checkerboard floor moved by the palette alone, a flat-shaded icosahedron on double-buffered playfield A, a sine scroller made of blitter copies, twelve orbiting ball sprites | an original 4-channel MOD (`amiga.mod`) |
| Finale | in time with the music: a CPU texture-mapped tunnel through the T1 logo at low resolution (after the FTA demos on the IIGS), then a palette-cycled 256-colour vortex warped by line scroll, a CPU texture-mapped cube, 64 depth-sorted sphere sprites, the T1 strobing on the sprite buffer layer, blended text panels | "T1 Rush", an original 8-channel MOD (`finale.mod`) whose piano runs speed up from 150 to 255 BPM over drum and bass, with SID-style chip voices flooding all 32 voices at the climax and a 24-voice supersaw at the end |

```sh
make -C apps/demo80s run                 # build and run it on the Tang (tang/tools/run.py)
make -C apps/demo80s shot ONLY=4 AT=1000 # section 4 in fsim, stopped 1000 ticks (50 Hz) in: out/s4_1000.png
make -C apps/demo80s music               # regenerate the modules (src/music_mods.h)
```

It needs the Tang's video extensions and blitter (docs/io.md, "Video extensions", "Blitter") and its audio
engine (docs/audio.md). Test builds: `-DONLY=n` runs one section, `-DSTOP_AT=t` stops fsim at tick t, `-DFPS`
prints each animated part's frame rate on the console every 2 s (and the cube's CPU cycles).

## How

- **One program**, `src/main.c` including `hw.h` (video, blitter, sprites, palettes, drawing), `music.c` and the
  four sections; the MOD replayer is `../modplay/src/mod.c` (now 4, 6 or 8 channels).
- **Music** runs from `poll()`, which the drawing calls while it waits for a frame or a flip and inside long
  loops; each player steps on the audio engine's 48 kHz frame counter, so ticks stay on time under load. The
  MOD uses engine voices 0-7, the SID tune 8-10, the beeper 12-13, the chip-voice effects and flood 8-31, the
  supersaw 16-31. The finale's scenes and flashes follow the replayer's song position and rows.
- **Memory**: the program and its data below 0x40000 (stack top), three 640x360 8 bpp regions at 0x040000,
  0x078800 and 0x0B1000, scratch from 0x0E9400 (sprite images, the scroller strips, the tunnel's tables and
  textures, the vortex's colour ramps, the line-scroll table), the text layer at 0x0FC000; the finale's 4 bpp
  sprite buffer goes right after the program (the Makefile checks there is room for it and the stack).
- **What is cheap**: anything the palette can do (raster bars, the floor, the vortex, colour cycling), the
  hardware sprites (a register write a move), line scroll (a table), and the blitter (clears, the tunnel's row
  doubling, the scrollers, the cube's row doubling).
- **What the CPU does**: the Apple part's lines, the Amiga object's spans (30 frames a second), the tunnel's
  14,400 texels a frame (20 a second) and the cube's affine texture mapping at 2x2 pixels (16-17 a second, 13-15
  in the climax). On the board a T1 instruction costs a few cycles and a loop iteration about 15, so the inner
  loops are unrolled, use byte stores rather than a multiply to double a byte, and the cube's texture gradient is
  worked out once a triangle (no division a span).

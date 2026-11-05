# The Future

A ten-minute demo for the Tang Nano 20K. People fly jets, then rockets, land on the Moon, build a space
station and a Moon base, cross the solar system and leave for the stars. The computer drawing it grows
with them, from a one-bit 128 x 72 screen and a beeper to everything the Tang has. The film starts as video
games and ends as cinema, with credits.

Everything is original and made by the program: the pictures are drawn as it runs, and the music is
sequenced and played on the audio engine (the drum samples and waveforms come from `tools/mkassets.py`).

| Phrases | Time | Era | Pictures | Sound |
|---|---|---|---|---|
| 0-4 | 0:00 | Jets | 1-bit at 128 x 72 (the title, the take-off), 160 x 90, then colour one entry at a time (CGA's four), 213 x 120 for the supersonic climb | one square voice, then two |
| 5-9 | 1:15 | Rockets | the pad and the countdown, ignition (six colours), the climb with counters, staging over the Earth at 320 x 180, the coast to the Moon | a sound chip (pulse, triangle, noise), then a SID (envelopes, PWM, filter) |
| 10-15 | 2:30 | The Moon | Apple II hi-res colours: a lunar-lander descent with instruments and the zoom near the ground, the landing, the flag, the walk, the earthrise; then 16 colours | SID |
| 16-21 | 4:00 | The station | 16 and then 32 colours: the Earth's limb, three star layers on a second playfield (line scroll), the station built module by module, sprites for the astronaut, the sun's flare and the shuttle | four sample channels, Amiga style |
| 22-26 | 5:30 | The Moon base | an isometric diorama in 256 colours, 320 x 180 and then 640 x 360; rover, lander and capsule as sprites; night by the palette | eight channels |
| 27-32 | 6:45 | The planets | 640 x 360, letterboxed: textured rotating planets (Mars, Jupiter and its moons, Saturn and its rings, Neptune), the asteroid belt, the edge | sixteen voices |
| 33-36 | 8:15 | The stars | the starship, the launch, the jump (a palette tunnel bent by line scroll), a new world | all 32 voices |
| 37-39 | 9:15 | Credits | a spiral galaxy turning behind credits scrolled on the second playfield | |

```sh
make -C apps/future run                     # build and run it on the Tang (tang/tools/run.py)
make -C apps/future shot START=27 AT=3500   # fsim from phrase 27, stopped at 16th 3500: out/p27_3500.png
make -C apps/future DEFS=-DFPS run          # frames a second per phrase on the console
make -C apps/future assets                  # regenerate src/assets.h
```

## How

- **One clock.** The music (`src/seq.c`) is a 16th-note sequencer paced by the audio engine's 48 kHz frame
  counter (a 16th at 128 BPM is 5,625 frames exactly). The scenes read the song position (`mpos()`) and
  draw what that moment shows, so they keep time with the music at any frame rate and can start anywhere.
- **The music's levels.** Each phrase has a level and the parts that play. The level picks the instruments:
  the beeper's kick is a click on the one voice, the sound chip's a pitch drop on the noise voice, the SID
  adds envelopes and the filter, and from level 4 the parts are samples (kick, clap, hats, crash) and
  256-frame waveforms played at the chip voices' steps. Levels 5-7 add echoes, a sub bass, supersaw pads
  through the filter with sidechain pumping, and a five-saw lead.
- **Resolution.** `src/cv.h` draws into a window of an 8 bpp buffer at a block size: a "pixel" is a blitter
  fill. The scenes draw in screen units, so a shape keeps its size while the pixels get finer.
- **3D** (`src/g3d.h`) is flat-shaded: meshes, a camera (perspective or orthographic), depth buckets. It is
  used for pictures drawn once (the Moon base, the starship, its sprites), never every frame: the base is
  painted a few buckets a frame while the low-resolution version is showing.
- **The planets** are a precomputed sphere (per pixel of the disc: longitude and a dithered light level)
  over a 128 x 64 texture, 8 tones x 16 light levels in the palette, drawn at half resolution with the rows
  doubled by the blitter; the camera moves the planet with the scroll register.
- **Memory.** The program and its data below 0x40000; three 640 x 360 buffers at 0x040000, 0x078800 and
  0x0B1000; the samples (copied to SDRAM, where the engine reads) and sprite images in the scratch area
  from 0x0E9400; the text layer at 0x0FC000.

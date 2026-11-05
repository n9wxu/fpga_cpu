# Overdrive

A high-energy demo for the Tang Nano 20K, about 3 minutes 20 seconds. It takes its manner from the rave
demos of the early 90s: hard cuts on the beat, flashes and shakes on the kick, strobes on the clap, a
silhouette dancer, vector balls, a tunnel, a plasma, raster bars and a sine scroller, builds that cut
faster and faster into a white-out, and an ending on one last hit. Everything is original: the pictures
are made as it runs, and the music is sequenced and played on the audio engine (the drum samples and two
waveforms come from `tools/mkassets.py`).

| Phrase | Time | Picture | Music |
|---|---|---|---|
| 0 | 0:00 | a word a beat, stripes slamming in on the 16ths, a countdown | kick and acid; hats, claps, a snare roll, a riser |
| 1-2 | 0:13 | the plasma: colour schemes a bar, the logo bouncing, 40 vector balls morphing, shake and flash on the kick, inverted on the clap | the drop: stabs, a galloping bass, then the 5-saw lead |
| 3-4 | 0:38 | the tunnel bent by line scroll; balls, then the logo on a ring | the lead and a piano arpeggio |
| 5 | 1:04 | raster bars, a sine scroller of sprite letters | the breakdown: pad, piano, the lead |
| 6 | 1:17 | cuts between the three fields, every bar, then beat, 8th, 16th; white-out | the build: a snare roll speeding up to 32nds |
| 7-9 | 1:30 | the dancer over the plasma, then three, then the balls | the second drop |
| 10 | 2:08 | the acid break: the plasma waving, a new ball shape every bar | kick, stabs and the acid line |
| 11 | 2:21 | the build again | |
| 12-14 | 2:34 | the last drop: a cut every beat, the scroller, strobes, glitches | everything, and the pad |
| 15 | 3:12 | the logo on raster bars, the last hit, the credits | |

```sh
make -C apps/overdrive run                   # build and run it on the Tang (tang/tools/run.py)
make -C apps/overdrive shot START=7 AT=930   # fsim from phrase 7, stopped at 16th 930: out/p7_930.png
make -C apps/overdrive DEFS=-DFPS run        # frames a second per phrase on the console
make -C apps/overdrive assets                # regenerate src/assets.h
```

## How

- **The music** (`src/seq.c`) is a 16th-note sequencer at 150 BPM, paced by the audio engine's 48 kHz frame
  counter (a 16th is 4,800 frames exactly). 32 voices: drums from samples, a sampled bass and a triangle sub,
  an acid line (a saw through the resonant filter, its cutoff kicked by accents and swept by the phrase),
  rave stabs (4 notes x 2 saws), a pad that ducks under the kick, a piano arpeggio, a 5-saw lead that swoops
  into each note, and an echo. The pictures read the song position and the steps of the last kick, clap and
  crash, so every flash lands on its hit.
- **The fields** are pictures whose pixels are colour indices that the palette animates: a plasma (four sines,
  one of them of the distance from a point, built at half resolution during the intro), a tunnel (angle and
  depth from a quadrant precomputed by `mkassets.py`, mirrored) and raster rows (a colour per line pair, the
  bars drawn in the palette). A cut is a playfield address and a palette; line scroll bends and tears them,
  the scroll register shakes them. All of that is 60 frames a second.
- **Sprites**: the letters (the 8 x 14 font at 3 x 2, a gradient down each and a shadow) and 40 vector balls
  (5 shapes morphing, rotated and sorted by depth so the nearer ball has the lower sprite number).
- **The dancer** is the one thing the CPU draws every frame: limbs as quads and a head, posed on every beat
  from a routine and eased in over the first 16th, drawn at 2 x 2 by blitter spans into playfield B (R0 and R1
  in turn) over the plasma; the two smaller dancers either side are one drawn and one blitter copy. 30 frames
  a second for one, 20-24 for three.
- **Memory**: the program below 0x40000; the plasma in R2 (0x0B1000), the tunnel in R1 (0x078800), the rows in
  R0 (0x040000) (the intro and the dancer use R0 and R1 for themselves, and the tunnel and rows are rebuilt
  after them); the samples and the sprite images from 0x0E9A00.

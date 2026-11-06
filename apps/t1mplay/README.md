# t1mplay: T1M modules on the Tang

Plays T1M modules ([docs/t1m.md](../../docs/t1m.md)), a player format for tracker music on the
Tang's audio, on the microcoded audio engine ([docs/mcaudio.md](../../docs/mcaudio.md)), with a
tracker view on the text screen: the pattern around the playing row, each channel's instrument,
note and level, and the song's position, the nodes sounding, the tails and the engine's load.

A module says what the music is: notes, instruments built from oscillators, samples, pairs,
envelopes, LFOs, unison, sequences and filters, in physical units. It holds no microcode and
nothing of the engine's layout; `src/t1m.c` decides how the engine makes each feature (which routine
a voice runs, how many nodes it takes, what the engine does in hardware and what the player does a
tick at a time). Voices come from a pool of the engine's nodes: with the new-note action release, a
channel's last note rings on as a tail while its next one plays, until it dies away or the pool
needs its nodes.

```sh
make -C apps/t1mplay                     # build/demo.bin, sym40.bin, toccata.bin: the player with a song built in
make -C apps/t1mplay fsim SONG=toccata   # 8 s in fsim (--hw tang-mc): out/toccata.png, out/toccata.wav
make -C apps/t1mplay run SONG=sym40      # on the Tang (needs: cd tang && make mcaudio prog-mc)
make -C apps/t1mplay dump SONG=demo      # the module, printed back
```

A module at 0x100000 plays instead of the built-in one (`tang/tools/run.py build/demo.bin --data
0x100000:my.t1m`). Keys (serial console): space pause, `n` / `p` next / previous order position,
`1`-`9`, `0` mute channels 1-10, `q` quit.

## The songs

| Song | Source | |
|---|---|---|
| `demo` | `songs/demo.t1s` (written by `songs/mkdemo.py`; samples by `songs/mksamples.py`) | "Neon Circuit": every feature of the format: saw, triangle, pulse with PWM, noise, 8- and 16-bit samples, a hard-sync pair and a ring pair (a bell that rings on: tails), unison, all four kinds of sequence, two filters with LFO sweeps, most effects |
| `sym40` | `songs/convert.py sym40` | Mozart, Symphony No. 40, I, from `apps/sym40`: its 15 parts, sampled strings and horns, the winds through a low-pass |
| `toccata` | `songs/convert.py toccata` | Bach, Toccata and Fugue in D minor, complete, from `apps/toccata`: 12 parts on 24 channels (each part's flue rank and, at full organ, its reed), 24 pipe samples (6 timbres in 4 ranges, an instrument each); the pipes ring on after their keys (tails), a pipe played again speaks again |

The two conversions read the notes and samples from the apps' own generators and turn each engine
setting back into physical terms. Against the originals in fsim, the loudness over the first 12 s
follows to a correlation of 0.98 (sym40) and 0.96 (toccata), each within 2% in level.

## Files

| Path | |
|---|---|
| `tools/t1mc.py` | the compiler (`.t1s` to `.t1m`) and `--dump` |
| `src/t1m.c`, `src/t1m.h` | the player: loading, the engine mapping, the song, the effects; runs in the block-done interrupt |
| `src/main.c` | the app: the tracker view and the keys |
| `songs/` | the songs and their generators |

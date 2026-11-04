# modplay: an Amiga MOD tracker for the Tang Nano 20K

Plays 4-channel ProTracker MODs (`M.K.`, `M!K!`, `FLT4`, `4CHN`) on the audio engine (`docs/audio.md`) and shows
them as a tracker on the 640x360 video system: the song position, pattern, row, speed and BPM, a scrolling
4-channel pattern view with the playing row highlighted, the sample list (samples in use in their channel's
colour) and a panel per channel with its note, sample, volume, effect and a VU meter.

```sh
make -C apps/modplay                                  # build/modplay.bin (needs llvm/fcc)
make -C apps/modplay run MODS="a.mod b.mod"           # on the Tang: loads the image and the MODs, runs
make -C apps/modplay fsim MODS=a.mod                  # 300 ticks in fsim, as fast as it goes: out/screen.png
```

Loading is over the 115200-baud serial port: about 11 KB a second (a 350 KB MOD takes half a minute).

## Using it

| Input | Action |
|---|---|
| space | pause |
| `n`, `+`, Right / S1 | next song position |
| `p`, `-`, Left | previous song position |
| `t` / S2 | next MOD |
| `1`-`4` | mute / unmute a channel |
| `i` | linear interpolation on/off (default off: nearest sample, as the Amiga) |
| `s` | stereo: 50% (default), hard Amiga left/right, mono |
| `q` | exit |

Keys come from the serial console (`make run` starts `run.py -i`) and the keyboard queue.

## How it works

- **The file in SDRAM.** `tools/pack.py` places the MODs from 0x040000, each at the next 256-byte boundary after
  the last, and clears 2 KB after them; modplay walks the list until a signature is missing. Up to 0x0C3C00 (the
  bitmap): about 540 KB of MODs.
- **The replayer** (`src/mod.c`) is ProTracker 2: speed and BPM (`Fxx`), ticks timed by the engine's 48 kHz
  frame counter (2.5 / BPM s each), and the effects 0 arpeggio, 1/2 slides, 3 tone portamento, 4 vibrato,
  5/6 (3/4 with a volume slide), 7 tremolo, 8 panning, 9 sample offset, A volume slide, B position jump,
  C volume, D pattern break, F speed/tempo, and E1/E2 fine slides, E4/E7 waveforms, E5 finetune, E6 loop,
  E9 retrigger, EA/EB fine volume, EC cut, ED delay, EE pattern delay (E0 filter, E3 glissando, EF are ignored).
  Periods come from ProTracker's table for all 16 finetunes (`tools/gentables.py`).
- **The engine**: channel c is engine voice c. The samples play from the file in place (8-bit signed): `ADDR`
  the sample, `LOOP`/`END` its repeat (loop mode 1) or its length (one-shot), `STEP` = 4842694 / period (PAL),
  `VOL` = volume x 4 split by the channel's pan (Amiga: 0 and 3 left, 1 and 2 right). A note writes the
  position (which arms the voice), then the gate. `MASTER` is 0.5 so four full voices stay in range.
- **The screen** (`src/ui.c`): text through a shadow (only changes are written), the pattern redrawn when the row
  moves, the meters by their difference.

# sidplay: a SID tracker for the Tang Nano 20K

Plays C64 `.sid` tunes (PSID v1/v2) on the audio engine (`docs/audio.md`) and shows them as a tracker on the
640x360 video system: a scrolling three-column pattern view reconstructed from the SID register writes, an
instrument list, live per-voice state with ENV meters and sweeping scopes, the filter and the master volume.

```
 SID TRACKER  SONG 01/02  TUNE 1/3  TIME 00:07  PAL 50HZ  CPU [##    ] 12% L 0
 Neon Drift                      by fpga_cpu demo                    2026 original
 KEYS: 1-3 MUTE  S STEREO  N/P SONG  T TUNE  SPACE PAUSE    S1 SONG  S2 TUNE
 ROW NOT IN WAVE  FX | NOT IN WAVE  FX | NOT IN WAVE  FX |   INSTRUMENTS
 3F  ... .. ..... P66|                 |                 |   01 --P- 03 30 -- 1
 40  A-5 06 --P-- ...| A-2 02 -S--- ...| C-2 03 ---N- 267|   02 -S-- 08 84 -- 2
 41  ... .. ..... P61| ... .. ..... ...| ... .. ..... 276|   ...
 42> G-5 06 --P-- ...| ... .. ..... ...| F-6 04 ---N- ...|   (current row: highlighted bar)
 VOICE 1  [note, FRQ, PW, ADSR, wave, ring, sync, filter, VU bar, scope]  VOICE 2  VOICE 3   FILTER / MASTER
```

Screenshots from fsim are in `out/` (`screen.png`, `screen_*.png`).

```sh
make -C apps/sidplay                 # build/sidplay.bin (needs llvm/fcc)
make -C apps/sidplay fsim            # 400 frames in fsim: out/screen.png, per-frame statistics, engine register log
make -C apps/sidplay run             # on the Tang: loads sidplay.bin and the three demo tunes, runs (tang/tools/run.py, pyserial)
make -C apps/sidplay run TUNE=my.sid # your own PSID file in slot 0
make -C apps/sidplay host            # host build of the player core
make -C apps/sidplay audio          # engine-model and reference WAVs of the three tunes -> out/*.wav
```

## Using it

| Input | Action |
|---|---|
| S1 (BUTTONS bit 0) / `n`, `+`, Right | next song of the tune (wraps) |
| `p`, `-`, Left | previous song |
| S2 (BUTTONS bit 1) / `t` | next tune (the next slot that holds a PSID) |
| `1` `2` `3` | mute / unmute a SID voice |
| `s` | stereo spread on/off (default on, 50% of the voices' gain) |
| space | pause |
| `q` | exit (the EXIT register) |

Keys come from the serial console (`CON_IN`: `make run` starts `run.py -i`, which sends the terminal's keystrokes) and the keyboard
queue. Unsupported tunes (RSID, play address 0) are listed and show a red message instead of playing.

## Tunes: where they are

Four 64 KB SDRAM slots, `0x080000 + n * 0x10000`. A slot holds a PSID file when it starts with `PSID` and its
last four bytes (`+0xFFFC`, little endian) hold the file length. `tang/tools/run.py` writes both:

```sh
python3 tang/tools/run.py apps/sidplay/build/sidplay.bin --tune 0x080000:a.sid --tune 0x090000:b.sid --zero 0x0A0000:4
```

(`--data ADDR:FILE` loads a file, `--tune` is `--data` plus the length word, `--zero ADDR:N` clears N bytes; the
extra loads happen before the image runs, and run.py without them behaves as before.) The slots are not cleared
by the program: when the board has an older tune in a slot, clear it with `--zero`. If no slot holds a PSID, the
built-in tune (Neon Drift, `src/tune0.h`, 1.7 KB) plays. `make run` loads all three demo tunes.

The player does not look at RSID, and not at tunes whose play address is 0.

## What the tracker shows and how it is made

Every play() call makes a frame: the 25 SID registers after the call plus which voices saw a gate rise or fall
inside it (from the write log). Frames go into a ring; a *look-ahead* of 96 calls (1.9 s) is generated before
the audio is started and kept up while playing, so the pattern view can show the rows that are coming, as in a
tracker. The model (`src/model.c`) turns frames into rows:

- **Speed** (calls per row): every note start votes for the divisors (2..12) of its distance from the first one;
  the speed is the largest that 90% agree with (changed only if the current one drops under 80%). The first
  96 calls are scanned before they are shown, so row 0 is already on the grid. A tune that changes speed keeps
  its old rows (each row stores its own length).
- **Notes**: a gate rise starts a note (name from the frequency register: the nearest of the 96 PAL or NTSC
  notes, found by binary search over `src/tables.h`); a gate fall without a rise is `---`; a single pitch jump
  while gated is a legato note.
- **Instruments**: there is no instrument in a SID, so an instrument is the triple (waveform + ring + sync
  bits, AD, SR). They are numbered in order of first use, listed on the right with the voices that use them
  now (1 2 3).
- **Effects** per row, from the pitch in 1/256-semitone units: `0xy` arpeggio (offsets x, y in semitones),
  `1xx` / `2xx` slide up / down (xx = average step), `4xy` vibrato (x = swings, y = depth in 1/16 semitone),
  `Pxx` pulse-width sweep (xx = PW high byte).
- The row number is the absolute row (hex, wraps at 100); the highlighted bar is the row being heard.

The structures are `inst_t`, `cell_t` and `row_t` in `src/model.h`: a song as rows of cells that refer to
instruments, so an editor and a SID-driver exporter could be built on them later.

Screen: the text layer (80x25) carries the pattern and all the numbers through a shadow so that only changed
cells are written; the bitmap (hi-color, own palette) carries the panels, meters and scopes. Per frame only
the difference is drawn (meter changes, one scope column plus the sweep cursor, text cells that changed).
Colours: voice 1 yellow, 2 cyan, 3 magenta; notes white, instrument cyan, waveform green, effects magenta,
empty cells and far rows dark blue. ENV is the engine's own envelope level (`ENV`, word 15 of its voice) when
the engine is there (`ID` = 0xA032), else a software copy of the engine's envelope ("SIM ENV" in the
corner; that is what fsim shows).

## Mapping SID -> audio engine (`src/sidmap.c`)

The frame's end state is applied once per play call (sub-frame timing is dropped: see limitations).
Only registers that changed are written.

- **Voices.** SID voice v (0..2) plays on engine voice `2v+1`. The engine takes ring and sync from the voice
  before (n-1), but the SID takes them from 3 -> 1, 1 -> 2, 2 -> 3. Engine voice `2v` is therefore a *silent
  copy* of SID voice `(v+2) mod 3`'s oscillator: same step, saw, no envelope (`ENVEN` 0: full level), volume 0, with the gate on so
  the oscillator runs even when the SID voice is released (a SID oscillator never stops). It is enabled only
  while voice v has ring (with triangle) or sync set; otherwise its CTRL is 0 (idle, 3 clocks). The copy's own
  sync/ring is not reproduced (a modulator that is itself modulated runs free in the copy).
- **Pitch.** `step = Fn x clock / 48000` (the SID's 24-bit phase step per clock times clock / 48000 clocks per
  engine frame; PAL 985248 Hz: `Fn x 20.526`; NTSC 1022727 Hz from the PSID flags). Fn up to 65535 gives
  1.35 M < 2^24.
- **Waveform.** `SRC`: noise (0x80) > pulse (0x40) > saw (0x20) > triangle (0x10) for combined waves (the SID's
  combined waves are not modelled); none or TEST: SRC 5 (silent); TEST also resets the phase (`POS` write).
  Noise uses `NOISE` = 7: 23-bit LFSR, multi-level (SID style). `RING` is set with triangle, `SYNC` with any wave.
- **Pulse width.** `DUTY = 4096 - PW` (engine: high while phase < DUTY; SID: high while phase >= PW).
- **ADSR.** `ENVDIV` = 3: the engine ticks every 8 frames (6000 ticks/s). Attack time A ms (the SID's
  2 ms .. 8 s table) gives `ATTACK = 2^24 / (16 x 6 x A)` = 174763 / A, capped at 65535 (the shortest
  attack is then 2.7 ms instead of 2 ms). Decay and release (6 ms .. 24 s) are exponential in the engine:
  `DECAY = RELEASE = 65536 / (6 x 0.62 x T)` ticks^-1 = 17617 / T, minimum 1 (the longest, 24 s, becomes
  an 11 s time constant (a 65536-tick one) instead of 15 s). The 0.62 is the SID's: the 255 -> 0 decay takes
  about 3x its nominal linear time through the exponential stages, with the first e-fold in 161 of 757 steps.
  `SUSTAIN = nibble x 0x1111`. `ENVEN` is always on.
- **Gate.** The engine sees one gate level per frame, so a gate fall and rise inside one play call (a "hard
  restart" or retrigger) is shown as gate 0 in this frame and gate 1 in the next (one frame of delay, 20 ms),
  and a rise and fall inside one call as gate 1 now and 0 next frame.
- **Filter.** `FMODE` = `$D418` bits 6:4 (LP BP HP map straight to the engine's 0 1 2); `FILT` per voice =
  `$D417` bits 2:0 (voice 3 with `$D418` bit 7 "3 OFF" is silent unless filtered); `FCUT` from the 11-bit
  cutoff through a 129-point table of `2 sin(pi f/48000) x 32768`, f = 30 + 12000 (FC/2047)^1.25 Hz, limited to
  0x7000 and to 0.875 (2 - damping); `FRES` = 32768 / Q with Q = 0.707 + 0.25 x resonance (0..15). A 6581-ish
  compromise; real chips differ from each other.
- **Volume.** `MASTER` = `$D418` low nibble x 0xC03/15 (0.75 at 15). Each voice has gain 0x40 of 0x100.
- **Stereo.** Voice 1 leans left, 2 centre, 3 right: `L,R = 0x40 (1 -+ spread/512)` with spread 128 (+-25%);
  `s` toggles 128 / 0 (mono), `sm_spread` is the knob. Muting a voice is volume 0.
- Engine voices other than the six are set to idle at start, `ENVDIV` is written once.

### Checked

`tools/compare.py tune song seconds` renders each SID voice alone through the engine model
(`fpga/tools/audio_ref.py`, driven by the registers `sidmap.c` writes) and through `tools/sidref.py` (an
independent floating-point SID: 24-bit accumulators, the SID's rate counters and exponential dividers, ring, sync, a
Chamberlin filter), and compares per 100 ms the RMS and the rising-zero-crossing rate (the pitch). Results are
under "Verification". WAVs of the engine renders: `out/neondrift.wav`, `out/glassbells.wav`, `out/syncrider.wav`
(30 s each, stereo, through `audio_ref.py`), references `out/*-ref.wav` (mono, level not matched).

## Running the 6502 code (apps/sid6502)

`src/player.c`: `sid_reset`, `sid_load`, `call_routine(init, song-1)`, then one `call_routine(play)` per
frame, with `cpu.cycles` raised to the next frame boundary (19656 PAL, 17045 NTSC cycles; a PSID speed bit
means CIA timing: the timer A value init left in `$DC04/5` sets the period, 60 Hz if there is none). The SID writes
of the call (`sidlog`) become the frame. The JIT runs the code (`pl_jit`; the interpreter in the host build).
Pacing on the board uses the engine's `SCOUNT` (48 kHz frames; the fractional part of 957.6 frames per PAL
call is carried), in fsim `CYCLE`.

`apps/sid6502` got these optional build parameters (defaults unchanged, `make -C apps/sid6502 test` still
3175 / 3175 on the interpreter and the JIT, 3125 / 3125 stepwise, as before; `make host` 3175):
`-DSID_MEM_ADDR/SID_ATTR_ADDR/SID_LOG_ADDR` (the three big arrays at fixed addresses),
`-DJIT_DATA_ADDR` (the event table, hash and bit maps at a fixed address; cleared at the first run, since
SDRAM is not zeroed by the loader), `-DHSIZE` (hash size).

## Memory on the Tang

All of 000000-0FFFFF is SDRAM behind the system's 32 KB direct-mapped write-through cache (set = address & 0x7FFF).
Nothing is held in block RAM any more. Built with `-O2`. The program image (46.5 KB) and BSS (44.8 KB, of which
the JIT's translation cache `CODE_WORDS` = 20000 words = 40 KB, enough that no tune here flushes it after the first
fill) start at 0x1000 and end near 0x17500; the stack starts at 0x30000 and grows down (the program uses about
400 bytes; `tools/sizecheck.py` fails the build if less than 8 KB is left below 0x30000). Fixed regions (`src/layout.h`):

| Range | Use |
|---|---|
| 001000-0174xx | program, BSS (translation cache) |
| 030C00-031BFF | frame ring (128 frames x 32 bytes) |
| 032000-032FA0 | text-layer shadow |
| 033000-033C80 | tracker rows (160 x 20 bytes) |
| 034000 | parsed PSID headers |
| 035000, 038000-03EFFF | statistics, engine register log: test build only |
| 040000-04FFFF | 6502 RAM |
| 050000-051FFF | SID write log (1024 entries) |
| 054000-063FFF | 6502 address attributes |
| 066000-075080 | JIT event table (1200), hash (2048), bit maps |
| 080000-0BFFFF | tune slots 0-3 (64 KB each, unchanged) |
| 0C3C00-0FBFFF, 0FC000 | bitmap, text (as `docs/io.md`) |

Cache: the hot data bases are staggered so they do not share sets: 6502 RAM at set 0x0000 (zero page, stack page, the
tune at $1000), attributes at 0x4000, JIT tables at 0x6000, frame ring 0xC00, text shadow 0x2000, rows 0x3000. The
program's code (about 38 KB) and the JIT's output necessarily share sets with something; I did not try to tune that.

## Cost

fsim has no T1F timing model, so the numbers are instructions (the `MFSR sr3` retired counter) per frame, and the
estimate uses the demo's measured 6.5 MIPS (block-RAM code; `tang/README.md`). `make fsim` 400 frames, test build
(it also logs the engine writes, a little more than the real build), after the 96-frame look-ahead:

| Tune | instructions per frame, average | of which play + model | UI | load at 6.5 MIPS (20 ms) |
|---|---|---|---|---|
| Neon Drift, song 1 | 29,100 | 12,600 | 14,300 | 22% |
| Glass Bells | 24,600 | 12,200 | 10,400 | 19% |
| Sync Rider | 29,000 | 12,800 | 13,900 | 22% |

The worst single frames (a row redraw plus a cold JIT translation, or a flush) are 50-100 k instructions,
7-14 ms at 6.5 MIPS: they fit a frame (the counters are 16 bits per part, so a part above 65 k wraps and the true
worst case can be higher; the translation of new code is also paid in these frames). The 6502 side is dominated by the SID-write events of the JIT (about 27 a frame
at several hundred cycles; the drivers write `sta $D400,y`, which the JIT cannot resolve at translation time). The pre-roll
(96 calls) before the sound starts is about 0.3-0.5 s. Where the time could be saved: inline SID-write
logging in the JIT (README of apps/sid6502), fewer scope pixels.
This is an estimate: nothing here has run on a T1F, and the cache's hit rate for the JIT's output, the 6502 RAM and the
UI's SDRAM bytes decides the real MIPS (misses cost 10-14 clocks per 16-byte line); the margin to 20 ms is
about 3x. If it turned out to be too slow, the first things to cut are the scope (3 x 88 byte writes a frame),
the per-frame text, and the per-frame text.

## Verification

- `make -C apps/sid6502 host`: 3175 passed; `make -C apps/sid6502 test`: interpreter 3175 / 3175, JIT 3175 / 3175,
  stepwise JIT 3125 / 3125 (same as before the placement options were added).
- **Target code = host code.** The engine register writes of the test build in fsim (the JIT running on the T1F
  code, 6502 RAM in SDRAM) were compared with the host build (interpreter, same `sidmap.c`): identical, all
  1364 writes of 400 frames of Neon Drift, and Glass Bells (1113) and Sync Rider (1678) for 400 frames.
- **Mapping against an independent SID** (`tools/compare.py`, 12 s of each tune, each SID voice alone, engine
  model vs `sidref.py`): pitch (zero-crossing rate per 100 ms) matches in 85/89 (Neon Drift voice 1), 118/119
  (voice 2), 119/119 (Glass Bells voice 1, the drone), 97/119 (pad, vibrato and band-pass), 117/119 (Sync Rider
  voice 1, the synced saw) and 119/119 (voice 3) windows; the noise voices and the ring-modulated bells are not
  comparable by zero crossings (noise; the product of two oscillators depends on their relative phase, which is
  arbitrary in both models; after fixing the reference's ring, 44/76 windows agree). Levels: the median
  engine/reference RMS ratio is 4600-7800 for the tonal voices (a constant gain of the two scales), 10700 for the ring bells,
  so the voices come out within about +-40% of each other relative to the reference. The RMS envelopes
  correlate 0.95-0.98 where the level moves (the lead, the synced saw, the pad) and 0.5-0.7 for the sustained
  or filter-dominated ones (flat signals correlate poorly). Clipping: 9 samples of 2.9 M in Neon Drift, none in
  the others.
- Loader and clock paths in fsim (`tools/mktests.py`, `tests/`): PSID v1 header, NTSC flag (60 Hz, NTSC
  note table), CIA-timed tune (59.99 Hz from `$DC04/5`), RSID and play = 0 (listed, red message, no sound),
  the built-in tune with no slot, tune and song switching by key (`out/screen_*.png`).
- Not verified: anything on the board (the engine, the buttons, `SCOUNT` pacing, the real ENV read, T1F
  speed, the cache behaviour), listening to the WAVs (the engine model is
  bit-exact to the RTL, per `docs/audio.md`, but I did not listen), a tune other than the three here (no
  other PSID files were available), and the JIT on tunes with self-modifying code or decimal mode.

## Limitations

- RSID tunes and tunes whose play address is 0 (they install an IRQ handler) are not supported: no CIA/VIC
  interrupts, no `$01` banking, no KERNAL. Tunes need to be self-contained PSID code.
- Digis (sample playback through `$D418`, 4-bit volume writes at 4-8 kHz) are not played: a frame keeps only
  the last value; the JIT could run them but the log would need a sample-rate path. Multi-speed tunes (play
  called 2-4x per frame) work if the PSID speed bit and `$DC04/5` give the rate (the UI then runs at that
  rate: 100 Hz is about 4x the load); the PSID flags/`speed` bit is the only source of the rate.
- Sub-frame timing is lost: all of a play call's writes become one state, so fast arpeggios inside one call and
  tricks that depend on write order (waveform glitches, test-bit digis, filter "hiss") are gone. A gate
  fall+rise inside one call costs 20 ms (see Gate).
- Combined waveforms use one component (see Waveform); the 6581 filter and its distortion, the
  analogue envelope bugs (ADSR delay bug), noise lock-up after test, `$D41B/$D41C` read-back
  are not modelled (osc3/env3 read the last written value, a property of apps/sid6502).
- A modulator that is itself ring-modulated or synced is approximated (see Voices).
- Only one SID (PSID v2 `secondSID` is ignored); the PAL/NTSC clock comes from the PSID flags.
- The tracker's rows are inferred, not the driver's own: a tune whose notes do not sit on a regular grid
  (funk tempo) shows rows of the nearest regular speed, instruments are SID settings, not the driver's.
- fsim: no engine, so no sound and the CPU load shown is derived from fsim cycles (meaningless there).

## Files

| Path | |
|---|---|
| `src/main.c` | start-up, tune slots, pacing, input, the per-frame loop |
| `src/player.c`, `psid.c` | PSID header, init/play on the recompiler, frames |
| `src/sidmap.c`, `tables.h` | SID -> engine mapping, software envelope; tables from `tools/gentables.py` |
| `src/model.c` | tracker model: speed, notes, instruments, effects, rows |
| `src/ui.c` | palette, panels, text shadow, pattern view, meters, scopes |
| `src/hw_target.c`, `hw_host.c` | engine register write and counters; the host harness |
| `tools/mktunes.py`, `mktests.py` | the three original tunes and loader test variants (`tests/`) |
| `tools/audiorender.py`, `sidref.py`, `compare.py` | engine model renderer, reference SID, comparison |
| `tools/log2script.py`, `stats.py`, `mkimage.py`, `sizecheck.py`, `bin2c.py`, `gentables.py` | build and test helpers |

The tunes (`tunes/*.sid`, original, PSID v2): **Neon Drift** (2 songs: speed 6 and 4; saw bass, pulse lead with
PW sweep, minor/major arpeggios, vibrato, noise drums with pitch drop, a resonant low-pass sweep), **Glass Bells**
(ring modulation: a triangle drone on voice 1 modulates the triangle bells of voice 2; pulse pad with
vibrato and a band-pass sweep), **Sync Rider** (hard sync: the voice-1 saw synced by the voice-3 bass with a
rising slide, drums, high-pass sweep). Their driver (`tools/mktunes.py`) is a 6502 pattern/order-list player
with instruments (ADSR, pulse sweep, auto slide), slides, vibrato, arpeggios and a filter sweep; it is assembled
with `apps/sid6502/tools/m6502.py` and wrapped in a PSID v2 header.

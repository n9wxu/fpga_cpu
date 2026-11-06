# T1M: the tracker module format

A player format for music on the Tang's audio: songs in patterns of rows, instruments built from
oscillators, samples, oscillator pairs, envelopes, LFOs and filters. A `.t1m` file says **what the
music is**, in physical terms (notes, cents, milliseconds, hertz, levels), never how an engine makes
it: there is no microcode, no routine or register number and no engine memory layout in it. The
player owns all of that. `apps/t1mplay` plays T1M on the microcoded engine (docs/mcaudio.md); a
player for another engine maps the same features onto what that engine has.

Files are made from a text source (`.t1s`, below) by `apps/t1mplay/tools/t1mc.py`; `t1mc.py --dump` prints
a `.t1m` back.

## What a song can use

| Feature | |
|---|---|
| Channels | 1-32, each plays one note at a time (a voice); a note can use several engine voices (unison, pairs) |
| Sources | saw, triangle, pulse (width), noise (pitch = its clock), samples (8- or 16-bit, mono, one-shot or looped, any rate), oscillator pairs: hard sync (two saws, the second restarted by the first) and ring modulation (two triangles, the second's sign flipped by the first's), with or without sync |
| Envelope | attack (linear, a time), decay and release (exponential, time constants), sustain level |
| Modulation | one sine LFO a voice: vibrato depth and pulse-width depth, at its rate |
| Unison | 1-4 voices a note, detuned across a spread in cents, panned across a width |
| Sequences | per-tick tables an instrument steps through from each note: volume, arpeggio, pitch, pulse width; each with a loop point and a release point |
| Filters | up to 4 state-variable filters (low-pass or band-pass, cutoff, Q), each with its own LFO sweeping the cutoff, output level and pan; every voice routed to the filters is summed into all of them (the filters run in parallel) |
| Mix | stereo volume and pan a voice, a global volume, the song's mix level |
| Effects | two effect columns a cell: slides, portamento, arpeggio, vibrato, PWM, width, volume and pan, sample offset, cut, delay, retrigger, key off, routing, pair detune, the filters' every parameter, flow (jump, break), speed and tempo |

A player tells the song what it cannot do: the header's `needs` bits (below) list the features used,
and the compiler reports the song's worst-case load in engine voices.

## Timing

A **tick** is the player's unit of time; the song sets the tick rate in hertz (`tick`, default 50)
and the ticks a row (`speed`, default 6). Notes start on a row's first tick (or later, with a delay
effect); effects and sequences advance every tick. The audio itself (envelopes, LFOs, filters,
pitch within a tick) runs continuously in the engine.

## Units

| Quantity | Unit |
|---|---|
| note | MIDI number: 60 = C-4 (middle C), 69 = A-4 = 440 Hz |
| pitch offsets | cents (1/100 semitone), or 1/16 semitone in per-tick effects |
| level | 0-255 (255 = 1.0, linear) unless noted |
| pan | 0 = left, 128 = centre, 255 = right; a voice at centre plays at its full level in both channels, toward a side the other channel fades linearly to 0 |
| time | milliseconds; decay and release are time constants (the level falls to 37% of the way to the target in that time) |
| LFO rate | hertz × 256 |
| cutoff | hertz; in effects, a byte c: 20 Hz × 2^(c/24) (c = 0: 20 Hz, 96: 320 Hz, 168: 2.56 kHz, 216: 10.2 kHz) |
| Q | × 256 (0.5 to 64); in effects, a byte q: Q = 0.5 × 2^(q/32) |
| pulse width | 0-65535 = 0-100% of the period (in effects, 0-255) |

## The binary format

Little endian. The file is a header and chunks; a chunk is a 4-character ID, a 32-bit length (of
its data), the data, then padding to a multiple of 4 bytes. Players skip chunks they do not know.
The file must be loaded at a 4-byte aligned address (sample data is played in place).

### Header (16 bytes)

| Offset | Size | |
|---|---|---|
| 0 | 4 | `T1M` 0x1A |
| 4 | 2 | version: 1 |
| 6 | 2 | `needs`: features used. Bit 0 noise, 1 samples, 2 16-bit samples, 3 sync pairs, 4 ring pairs, 5 filters, 6 unison, 7 sequences |
| 8 | 2 | the most engine voices sounding at once (unison and pairs counted) |
| 10 | 2 | reserved, 0 |
| 12 | 4 | total file length |

### `SONG` (16 bytes): the song

| Offset | Size | |
|---|---|---|
| 0 | 1 | channels (1-32) |
| 1 | 1 | speed: ticks a row |
| 2 | 2 | tick rate, hertz × 256 |
| 4 | 2 | mix level, × 256 (1.0 = a voice at full level fills the output) |
| 6 | 1 | global volume (0-255) |
| 7 | 1 | restart: the order position the song loops to |
| 8 | 2 | orders |
| 10 | 2 | patterns |
| 12 | 1 | instruments |
| 13 | 1 | samples |
| 14 | 1 | filters (0-4) |
| 15 | 1 | sequences |

### `NAME`: text (optional)

Three NUL-terminated strings: title, author, comment (ASCII).

### `ORDR`: the order list

One byte a position: the pattern played there.

### `FILT`: the filters (16 bytes each)

| Offset | Size | |
|---|---|---|
| 0 | 1 | mode: 0 low-pass, 1 band-pass |
| 1 | 1 | pan |
| 2 | 2 | cutoff, Hz |
| 4 | 2 | Q × 256 |
| 6 | 2 | LFO rate, Hz × 256 |
| 8 | 2 | sweep depth: the LFO moves the cutoff up and down by this many cents |
| 10 | 1 | output level |
| 11 | 5 | reserved, 0 |

### `SMPL`: the samples (20 bytes each), and `SDAT`: their data

| Offset | Size | |
|---|---|---|
| 0 | 4 | offset of the data in `SDAT` (a multiple of 4) |
| 4 | 4 | length, frames (at most 32764, a multiple of 4) |
| 8 | 4 | loop start, frames (a multiple of 4) |
| 12 | 4 | loop length, frames (0 = one-shot; else a multiple of 4, at least 256) |
| 16 | 2 | rate: Hz at the base note |
| 18 | 1 | base note |
| 19 | 1 | flags: bit 0 16-bit (else 8-bit); signed, mono |

`SDAT` holds the frames, each sample's at a 4-byte boundary, followed by its loop's first 256 frames
again (the guard: a player may read past the loop end into it; for a one-shot, 256 frames of
silence). The compiler makes short and ping-pong loops legal by unrolling them.

### `INST`: the instruments (64 bytes each)

| Offset | Size | |
|---|---|---|
| 0 | 16 | name (NUL-padded) |
| 16 | 1 | source: 0 saw, 1 triangle, 2 pulse, 3 noise, 4 sample, 5 pair |
| 17 | 1 | flags: bit 0 to the filters, bit 1 pair hard sync, bit 2 pair ring modulation, bit 3 free-running LFO (else it restarts at each note) |
| 18 | 1 | sample (source 4) |
| 19 | 1 | level |
| 20 | 1 | pan |
| 21 | 1 | sustain level |
| 22 | 2 | attack, ms |
| 24 | 2 | decay, ms |
| 26 | 2 | release, ms |
| 28 | 2 | pulse width |
| 30 | 2 | LFO rate, Hz × 256 |
| 32 | 2 | vibrato depth, cents |
| 34 | 2 | PWM depth (pulse-width units: the width swings this far each way) |
| 36 | 2 | pair: the second oscillator's offset, cents (signed) |
| 38 | 1 | pair: the second oscillator's level |
| 39 | 1 | unison voices (1-4) |
| 40 | 2 | unison detune spread, cents (between the outermost voices) |
| 42 | 1 | unison pan width (0 = all at the instrument's pan, 255 = hard left to hard right) |
| 43 | 1 | transpose, semitones (signed) |
| 44 | 1 | fine tune, cents (signed) |
| 45 | 3 | reserved, 0 |
| 48 | 8 | sequences: volume, arpeggio, pitch, width (a byte each: the sequence, or 0xFF = none), 4 bytes reserved (0xFF) |
| 56 | 8 | reserved, 0 |

### `SEQS`: the sequences

A table of 16-bit offsets (one a sequence, from the start of the chunk's data), then each
sequence: kind (1 byte: 0 volume, 1 arpeggio, 2 pitch, 3 width), length (1-255), loop (a step
index, or 0xFF: hold the last), release (a step index, or 0xFF), then the steps, a byte each. While
the note is held the sequence plays up to the release point (or its end) and goes round from the
loop point; after a key off it carries on from where it is past the release point to the end, and
holds the last step. Volume steps scale the voice (0-255); arpeggio steps add
signed semitones; pitch steps add signed 1/16 semitones, accumulating (a step of +4 each tick is a
slide up a quarter tone a tick); width steps set the pulse width (0-255). A sequence takes one step a
tick from the note's start.

### `PATT`: the patterns

A table of 32-bit offsets (one a pattern, from the start of the chunk's data), then each pattern: its
rows (2 bytes, 1-256), then the row stream:

| Byte | |
|---|---|
| 0x00 | end of the row |
| 0x01-0x20 | a cell in channel byte − 1, then a flags byte and the cell's fields |
| 0x80-0xFF | end of the row, and (byte − 0x80) more empty rows |

The flags byte: bit 0 note, 1 instrument, 2 volume, 3 effect 1, 4 effect 2. Then the fields in that
order: note (1 byte: 0-127, or 0x80 key off: release, 0x81 cut: silence now), instrument (1 byte,
from 0), volume (1 byte), each effect (command, parameter: 2 bytes).

A note without an instrument plays the channel's last instrument; an instrument without a note
resets the voice's volume to the instrument's level (as MOD). The volume sets the channel's level,
which multiplies the instrument's.

## Effects

Parameters are a byte; `x` and `y` are its high and low nibble. "Per tick" effects run on every
tick of the row but the first.

| Cmd | | |
|---|---|---|
| 00 xy | arpeggio | the note, +x, +y semitones, cycling every tick |
| 01 xx | slide up | xx/16 semitone per tick |
| 02 xx | slide down | |
| 03 xx | portamento | to the cell's note (which does not restart the voice), xx/16 semitone per tick |
| 04 xy | vibrato | LFO rate x/2 Hz, depth y × 8 cents (0: keep) |
| 05 xx | pulse width | xx × 256 |
| 06 xy | PWM | LFO rate x/2 Hz, depth y × 256 width units (0: keep) |
| 07 xy | width slide | up x × 64, down y × 64 width units per tick |
| 08 xx | pan | |
| 09 xx | sample offset | start xx × 256 frames in |
| 0A xy | volume slide | up x × 4, down y × 4 per tick |
| 0B xx | jump | to order position xx (at the row's end) |
| 0C xx | volume | the channel's level |
| 0D xx | break | to row xx of the next order position |
| 0E xx | fine tune | xx signed cents |
| 0F xx | speed | ticks a row |
| 10 xx | tempo | tick rate, xx Hz |
| 11 xx | global volume | |
| 12 xx | cut | silence at tick xx |
| 13 xx | delay | the note starts at tick xx |
| 14 xx | retrigger | the note again every xx ticks |
| 15 xx | key off | release at tick xx |
| 16 xx | routing | 0 straight to the mix, 1 through the filters |
| 17 xx | pair offset | the second oscillator xx/4 semitones (signed) from the first |
| 18 xx | pair slide | the second oscillator's offset moves xx/16 semitone (signed) per tick |
| 2u xx | cutoff | filter u (0-3), byte scale |
| 2(4+u) xx | cutoff slide | filter u, xx/8 (signed) of the byte scale per tick |
| 2(8+u) xx | Q | filter u, byte scale |
| 2(C+u) xx | sweep depth | filter u, xx × 8 cents |
| 3u xx | sweep rate | filter u, xx/16 Hz |
| 3(4+u) xx | filter level | filter u |
| 3(8+u) xx | filter mode | filter u: 0 low-pass, 1 band-pass |

## The text source (`.t1s`)

Line based; `#` starts a comment. Numbers are decimal unless noted; levels, pan and width accept
`0.0-1.0` (or `50%`).

```
title  "Neon Circuit"
author "fpga_cpu"
channels 10
tick 50            # Hz
speed 6            # ticks a row
mix 0.9
volume 1.0
restart 1          # the order position to loop to

sample kick "samples/kick.wav" note C-4            # 8- or 16-bit mono WAV; rate from the file
sample pad  "samples/pad.wav"  note A-3 loop 1024 4096 [pingpong]

filter 0 lowpass cutoff 1800 q 3 lfo 0.25 sweep 1200 level 0.9 pan 0.5

instrument 01 "Lead" pulse
  level 0.7 pan 0.5 attack 5 decay 300 sustain 0.6 release 200
  width 0.5 pwm 0.3 lfo 5.5 vibrato 12 filter on unison 2 detune 8 spread 0.3
  transpose 0 fine 0 freelfo off
  seq volume 255 230 200 | 180 / 120 60 0      # '|' the loop point, '/' the release point
  seq arp 0 4 7 |
instrument 02 "Sync" sync  offset 700  partner 0.8     # pair: hard sync; 'ring', or 'sync ring'
instrument 03 "Kick" sample kick

pattern 00 64
00 | C-4 01 C0 0A04 .... | --- .. .. .... .... | ...
04 | === .. .. .... .... |
order 00 01 01 02
```

A pattern is its rows; rows may be left out (empty). A row is `RR |` (hex) and a cell per channel,
`|` between: note (`C-4`, `C#4`, `===` key off, `^^^` cut, `---` none), instrument (hex), volume
(hex), two effects (`CCPP` hex, `....` none). Cells after the last one written are empty.

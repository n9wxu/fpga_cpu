# The audio engine

32 voices, stereo, 48 kHz 16-bit out. It plays tracker samples (MOD, S3M, XM
style), sampled sound effects and streams, and chip-style voices (pulse,
triangle, saw, noise, ADSR, ring modulation, hard sync, a resonant filter).
The CPU sets up notes at 50-60 Hz; the engine does every per-sample step.

Files: `fpga/rtl/audio_engine.v` (the engine), `fpga/rtl/audio_regs.v` (the
CPU register port), `fpga/sim/audio_tb.v`, `fpga/tools/audio_ref.py` (bit-exact
Python model), `fpga/tools/audio_tests.py` (the regression).

## How it fits

```
CPU --(I/O writes/reads)--> audio_regs --> audio_engine --> s_valid/s_l/s_r --> HDMI audio FIFO
                                              |
                                        rammux client port (read only, bursts of 8 words)
```

- One clock, the system clock (67.5 MHz on the Tang). No 48 kHz timer: the
  engine makes a sample when the FIFO has room (`s_ready`), so the FIFO's
  exact 48 kHz pop paces it.
- One sample ("frame") = the 32 voices one after another (3 clocks for an
  idle voice, 29 for a playing one), then the filter and mix (~17 clocks).
  Measured with all 32 voices playing: 914 clocks a frame, of 1406 available.
- The sample data is in RAM, reached through `rammux` (the one memory
  interface: `fpga/rtl/rammux.v`). The engine is one read-only port, the
  lowest priority. Each voice has a 16-word ring buffer (block RAM) that
  bursts of 8 words keep ahead of the playing position, so the engine uses a
  few percent of the RAM's time (a stereo 16-bit voice at 48 kHz reads 48,000
  words a second; 8 words a burst).
- Sample RAM addresses are **byte addresses in the rammux address space**
  (24 bits; word address = byte address / 4; little endian). On the Tang that
  is the SDRAM (CPU addresses 010000 and up; the 64 KB of block RAM at 000000
  is not reachable by the engine). On a board with SRAM, the SRAM.

## Registers

The engine's registers are 16 bits wide, at even addresses, written low byte
then high byte like all of `docs/io.md` (the high byte commits). The power-on
value of every register is 0, except `MASTER` (0x1000).

I/O addresses (relative to F00000): global registers at **F01000**, voices at
**F01800**, 64 bytes (32 words) each: word *w* of voice *v* is at
`F01800 + 64*v + 2*w`. The old 3-voice device at F00400 keeps working (below).

### Global registers (F01000)

| Address | RW | Name | Function |
|---|---|---|---|
| F01000 | RW | `MASTER` | master volume, 0x1000 = 1.0 (max 0xFFFF = 16x). The mix is the sum of all voices: with many voices lower it |
| F01002 | RW | `FMODE` | filter outputs added to the mix: bit 0 low-pass, 1 band-pass, 2 high-pass (0 = filtered voices are silent; 5 = notch) |
| F01004 | RW | `FCUT` | filter cutoff F: 2 sin(pi fc/48000) x 32768 (about fc x 4.29 up to a few kHz; keep below 0x7000) |
| F01006 | RW | `FRES` | filter damping Q: 1/Q x 32768 (0xFFFF = no resonance, 0x4000 = Q 2, 0x1000 = Q 8) |
| F01008 | RW | `ENVDIV` | envelopes advance every 2^n frames (n = 0-7): slows every attack, decay and release |
| F0100C / F0100E | RW | `IRQ_LO/HI` | read: voices with an interrupt flag (bit n = voice n); write 1s to clear those |
| F01010 / F01012 | R | `SCOUNT` | frames made so far (32 bits); the CPU can wait on it for a few frames of time |
| F01014 | R | `ID` | 0xA032 |
| F01018 | R | `UNDERRUNS` | count of frames in which a playing voice's data was not there yet |

### Voice registers (word w of a voice)

| w | Name | RW | Function |
|---|---|---|---|
| 0 | `CTRL` | RW | bits below |
| 1 | `STEP_LO` | RW | pitch step, low 16 bits |
| 2 | `STEP_HI` | RW | bits 7:0: step bits 23:16. The step is 8.16 fixed point: sample frames per output frame (0x10000 = 1.0). For a chip voice it is the 24-bit phase step |
| 3 | `VOL_L` | RW | left volume, bits 8:0; 0x100 = 1.0 (up to 0x1FF = +6 dB) |
| 4 | `VOL_R` | RW | right volume, the same |
| 5 | `DUTY` | RW | pulse width, bits 11:0 (0x800 = 50%) |
| 6 | `ATTACK` | RW | envelope attack rate: the level rises by 16 x ATTACK (of 2^24) a tick |
| 7 | `DECAY` | RW | exponential decay rate: each tick the level loses level x DECAY / 65536 (time constant 65536/DECAY ticks) |
| 8 | `SUSTAIN` | RW | sustain level (16 bits; decay stops there) |
| 9 | `RELEASE` | RW | release rate, as `DECAY` |
| 10 | `NOISE` | RW | bits 1:0 length: 0 15-bit (NES long), 1 15-bit with the short tap (NES periodic, 93 steps), 2 17-bit (AY), 3 23-bit (SID). Bit 2: multi-level output (8 bits of the register, SID style; else 1-bit) |
| 11 | `POS_F` | RW | position fraction (16 bits). Reads the live value |
| 12 | `POS_L` | RW | position, frame number bits 15:0. Reads the live value |
| 13 | `POS_H` | RW | position, frame number bits 23:16. **Writing this word commits the position** (write `POS_F`, `POS_L`, then `POS_H`). Reads the live value |
| 14 | `STATUS` | RW | read: bit 0 running, 1 done (a one-shot ended), 2 interrupt flag, 3 half (the position is in the second half of the loop), 4 underrun happened, 5 armed (prefetching). Write 1s to bits 1, 2, 4 to clear them |
| 15 | `ENV` | R | envelope level, 16 bits |
| 16 | `ADDR_LO` | RW | sample start, byte address bits 15:0 |
| 17 | `ADDR_HI` | RW | bits 7:0: byte address bits 23:16 |
| 18 / 19 | `LOOP_LO/HI` | RW | loop start, in frames from `ADDR` (24 bits) |
| 20 / 21 | `END_LO/HI` | RW | end: the loop end, or the end of a one-shot, in frames from `ADDR` (24 bits) |

Words 22-31 are spare (they store what is written). Everything above reads
back what was written, except words 11-15.

### CTRL

| Bits | Name | Meaning |
|---|---|---|
| 0 | `GATE` | 1 = note on. Rising: starts the envelope's attack (and a sample voice's playing). Falling: release (or silence at once without an envelope) |
| 1 | `ENVEN` | use the ADSR; 0 = the gate switches the voice on and off at full level |
| 4:2 | `SRC` | 0 sample from RAM, 1 pulse, 2 triangle, 3 saw, 4 noise (5-7 silent) |
| 5 | `RING` | triangle: ring modulation by the previous voice (voice n-1; voice 0 takes 31, one frame late) |
| 6 | `SYNC` | hard sync: the phase restarts to 0 when the previous voice's phase wrapped |
| 7 | `INTERP` | sample voice: linear interpolation between frames (0 = nearest frame, Amiga style) |
| 9:8 | `LOOP` | 0 none (the voice stops at `END`: `done`), 1 forward loop `LOOP`..`END`, 2 ping-pong |
| 10 | `B16` | 16-bit samples (else 8-bit) |
| 11 | `UNS` | unsigned samples (else signed) |
| 12 | `STEREO` | interleaved left, right frames (else mono: both channels) |
| 13 | `FILT` | send this voice through the filter (else straight to the mix) |
| 14 | `IRQEN` | raise the interrupt flag at the end of a one-shot and each time the position crosses the middle of the loop or wraps (streaming) |

Frame sizes: mono 8-bit 1 byte, mono 16-bit or stereo 8-bit 2, stereo 16-bit
4. A sample must be aligned to its frame size (16-bit data on even
addresses, stereo 16-bit on multiples of 4). 8-bit samples are placed in the
top byte of the 16-bit sample.

## What the engine does

For each voice, each frame:

1. The configuration is read; an idle voice (no gate, no envelope level, not
   armed) is skipped.
2. Gate edge, then the envelope. With `ENVEN=0` the level is 0 or full. With
   it, every 2^`ENVDIV` frames: attack adds 16 x ATTACK until full, then decay
   falls exponentially to `SUSTAIN`, held until the gate falls; release falls
   exponentially to 0. (Decay and release lose at least 1 of 2^24 a tick.)
3. The signal x (16 bits signed): a sample voice reads its frame at the
   position (8.16 frame number), and with `INTERP` adds (next - this) x
   fraction[15:8] / 256; a chip voice computes its waveform from the 24-bit
   phase: pulse (phase[23:12] < DUTY), triangle, saw, noise (an LFSR clocked
   when phase bit 19 rises). The phase is the position register's low 24 bits.
4. m = x x level[23:8] / 65536; the voice adds m x `VOL_L` to the left sum and
   m x `VOL_R` to the right (the stereo samples use their own channels).
5. The position advances by `STEP`. At `END`: one-shot stops (`done`); forward
   loop jumps back by the loop length; ping-pong reflects and reverses.
   (Step must be smaller than the loop.)

Then the mix: each channel has a direct sum and a filtered sum. The filtered
sum goes through a Chamberlin state-variable filter (24-bit state, per
channel, the same F and Q):

    low  += (band x F) >> 15
    high  = in - low - ((band x Q) >> 15)
    band += (high x F) >> 15

and the output is `sat16(((direct >> 8) + chosen filter outputs) x MASTER >> 12)`.
Everything above is bit exact in `fpga/tools/audio_ref.py`.

### Memory and timing rules

- **Arm a voice before its gate.** Writing `POS_H` (the position) arms the
  voice: it fetches its first words at once. The fetch takes under a frame, so
  a gate written in the same tick works too, but the voice then holds (silence
  from a fresh start) for a frame or two until the data is there. With many
  voices starting in one tick, the fetches queue one at a time (about 40
  clocks each).
- After changing `ADDR`, the format or the loop, write the position again (the
  voice forgets what it had fetched).
- The buffers read ahead (up to 16 words) and the first 8 words of a forward
  loop are kept, to wrap without a gap. A refill of the RAM the voice is about
  to play must be at least ~10 frames ahead. For a stream, refill the half
  just left (below).
- A voice can consume about 4 words a frame sustained (a stereo 16-bit voice
  up to 4x pitch, an 8-bit one to 16x); over that it underruns (it repeats its last
  frame and sets `UNDERRUN`).
- A write to a voice takes effect at that voice's turn in the frame; a 24-bit
  field written as two words can be seen half-changed for one frame.

### Streaming

A looped buffer with `IRQEN`: the engine sets the voice's flag (and the
`IRQ_LO/HI` bit) each time the position crosses the middle of the loop and
each time it wraps. `STATUS` bit 3 (half) says which half is playing: when it
is 1, the first half is free to refill, when 0 the second. Clear the flag
(write 4 to `STATUS`), refill, done. Doom's sound effects and PCM streams
(11 or 22 kHz up to 48 kHz stereo 16-bit) use this or a plain one-shot.

## Examples

C-like, `V(v,w)` the address of word *w* of voice *v* (F01800 + 64v + 2w).

**A MOD note.** Amiga period *p* (PAL): `step = round(4842694 / p)` (NTSC
4887272 / p). `STEP_LO/HI` = step & 0xFFFF, step >> 16. MOD volume 0-64 maps
to `VOL = vol x 4`. Amiga panning (channels 0 and 3 left, 1 and 2 right): the
other side's volume 0.

    W(V(c,0), CTRL_LOOP)            // gate off, loop on (a repeat sample)
    ADDR = sample start; LOOP = repeat start, END = repeat start + repeat length
    // (for a sample without a repeat: LOOP=0, loop mode 0, END = length)
    STEP = round(4842694 / period)
    VOL_L = vol*4, VOL_R = 0        // channel 0
    POS = 0                         // or the 9xx sample offset: POS_L = offset*256
    CTRL = GATE | LOOP(1)           // the same tick or the next

A slide changes `STEP` each tick; arpeggio changes it per tick; volume slides
change `VOL_L/R`; a retrigger is gate 0, position write, gate 1.

**A Doom sound effect** (8-bit unsigned, 11025 Hz, mono; DMX lumps carry an
8-byte header and 16 pad bytes before the samples): `ADDR` = lump + 24, `END`
= sample count, `LOOP` mode 0, `CTRL` = `UNS` (bit 11) | `INTERP` | `GATE`,
`STEP` = 11025 / 48000 x 65536 = 15053 (22050 Hz: 30106). Volume and
stereo position from the distance: `VOL_L`, `VOL_R`. Pick a free voice, or the
oldest; `STATUS` bit 1 (done) says a one-shot has finished.

**A chiptune pulse voice** (NES-like, 220 Hz, duty 25%, with an envelope):

    STEP = round(220 x 2^24 / 48000) = 76895   // 24-bit phase step
    DUTY = 0x400
    ATTACK = 0x2000; DECAY = 0x0100; SUSTAIN = 0x6000; RELEASE = 0x0200
    VOL_L = VOL_R = 0x100
    CTRL = SRC(1) | ENVEN | GATE   (= 0x0007), release by writing GATE = 0

Triangle: `SRC` 2 (with `RING` it is multiplied by the previous voice's phase
sign). Saw: `SRC` 3. Noise: `SRC` 4 with `NOISE` as above; the LFSR is clocked when phase bit 19 rises, so it shifts `step` x 48000 / 2^20 times a second (4 kHz: step 87381; at most once a frame). Filtering: set `FILT` on the voices, `FMODE`, `FCUT`, `FRES`.

**Reading the position** (to follow playback): read `POS_F`, `POS_L`,
`POS_H`; the value is that of the last finished frame.

## The old 3-voice device (F00400)

`SND_FREQ`, `SND_VOL`, `SND_WAVE` of voices 0-2 (8 bytes apart) work as in
`docs/io.md`; each write is turned into engine voice 0-2 settings: pulse
(50%), triangle, saw or noise (wave 0-3) at `FREQ` Hz, with volume `VOL` x 5
(0-15 -> 0-0x4B of 0x100, so three voices at full volume do not clip); gate
= `FREQ` and `VOL` both non-zero. It writes `CTRL`, `STEP`, `VOL`, `DUTY` of
the voice (six register writes taking 7 clocks), so a program using it should
not also use engine voices 0-2 at the same time.

## What io.md should get

A section "Audio engine (F01000)": the tables above (global, voice, CTRL), the
streaming and arming notes, and in "Sound (F00400)" the sentence "on the Tang
these are voices 0-2 of the audio engine". The "Memory layout" does not
change (the engine plays from SDRAM addresses).

## Hooking it into the Tang system

- Instantiate `audio_engine #(.AW(<rammux AW>))` in `tang/rtl/system.v` (or
  its RAM-machine successor), on fclk. Register port: `io_wr`, `io_addr`,
  `io_wdata` are the same signals that go to `iosys` (a one-clock write
  strobe with a stable address and byte); `rd_addr` = `cur_addr`. `rd_data` is
  16 bits, valid one clock after `rd_addr`, zero when the address is not the
  engine's.
- `iosys.v`: add an input `aud_rd[15:0]` and `wire [15:0] live = rv(cur_addr) | aud_rd;`
  (iosys already ignores writes to unknown addresses and has no sound yet).
  Its read register is sampled at `io_lat`, one clock after the address
  settles: that is what the one-clock `rd_data` needs. Nothing else changes.
- rammux: one more port (the lowest), `p_req/p_addr/p_len/p_ack/p_rvalid`
  from the engine, `p_we` 0, and the port's bit clear in `WMASK`. `p_len` is
  always 8. This needs the Tang's SDRAM behind a rammux PHY (today `sdram.v`
  is its own controller).
- HDMI: `s_valid`, `s_l`, `s_r`, `s_ready` to `hdmi_audio_cdc.v` (the FIFO's
  write side, on fclk); `s_ready` = the FIFO has room.
- `irq` (any interrupt flag) is interrupt source 3 of iosys's controller
  (docs/io.md, F00700): a handler clears the voice's flag (`IRQ_LO/HI`),
  then the source's pending bit. The flags can still be polled.

## Size and speed

`synth_gowin` (yosys), the engine alone with the register port, AW=22:
about 8k LUT4 + 1.7-2.3k ALU (carry) cells, 3.9k flip-flops, 6 block RAMs
(configuration x3, voice ring, loop heads, snapshot/LUT RAM), 5 18x18 and 4
9x9 multipliers. nextpnr place and route (GW2AR-18): 85 MHz, 67.5 MHz needed.

On the Spartan-3 Starter Board the engine does not fit (the XC3S200 has about
3.8k LUTs, 12 block RAMs and 12 multipliers; yosys puts the engine at about
7k LUTs and 8 block RAMs). The Spartan-3 has the low tier's sound instead
(`fpga/rtl/audio_lite.v`, docs/io.md "Sound"): the 3-voice device at F00400
and a streaming player, on an I2S DAC.

## Verification

`python3 fpga/tools/audio_tests.py OUTDIR [mod chip stream legacy stress]`
renders register scripts with `audio_ref.py` and with the RTL (iverilog,
`fpga/sim/audio_tb.v`: the real `rammux` over a behavioural RAM with gaps and
latency, and a competing port), compares bit for bit, and writes WAVs of both.
Tests: `mod` (5 voices: loops, ping-pong, one-shot with sample offset and
retrigger, pan, interpolation, 16-bit stereo), `chip` (pulse with ADSR, ring,
sync, two noises, filter in all modes), `stream` (a 512-frame looped stereo
buffer refilled by the CPU at each half, with interrupt flags), `legacy`
(F00400), `stress` (32 voices; it also reports the clocks a frame takes).

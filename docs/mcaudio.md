# The microcoded audio engine (Tang Nano 20K, `-DMCAUDIO`)

Issue #23's engine, built: a list walker that renders 32-sample blocks from a linked list of voice
nodes in the SDRAM, through one micro-sequenced datapath whose routines (the voices' "scripts" and
their per-sample loops) live in a 512-word control store the CPU loads. It replaces the fixed
engine (docs/audio.md) in a second bitstream: `cd tang && make mcaudio prog-mc`. The demo
(`apps/mcdemo`) plays each feature with a live diagram of the data flow.

Sources: `fpga/rtl/mcaudio/` (`mc_engine.v` walker, buffers, output and monitor; `mc_core.v` the
sequencer and datapath; `mc_regs.v` the CPU port), `fpga/tools/mcasm.py` (the assembler and the
routines), `fpga/tools/mctest.py` with `fpga/sim/mc_tb.v` (tests under iverilog with the real
rammux).

## How sound moves

```
CPU ──(writes node parameters, between blocks)──▶ SDRAM: node list, samples
                                                    │ rammux port 2 (lowest priority after video)
  ┌──────────────────────── audio engine ───────────┴──────────────────────────────────┐
  │ list walker ─ 16-word burst per node ─▶ node buffer (4 nodes: a linked group)       │
  │                                              │                                       │
  │ control store (512 x 72) ─▶ sequencer ─▶ datapath: RF 16 x 32, ALU, wave unit,      │
  │                                │           one 18x18 MAC, sine table               │
  │       DMA bursts ◀── SYS DMA ──┘           │ sample reads ◀── sample buffer (512 w)  │
  │                                            ▼                                         │
  │                                 mix buses: main L/R, filter L/R (32 entries each)    │
  │ state write-back (words 12-15) ─▶ SDRAM    │                                         │
  │                                            ▼ (after the list) saturate, >> shift      │
  │                                 output FIFO (64 stereo) ─▶ monitor ring              │
  └────────────────────────────────────────────┬─────────────────────────────────────────┘
                                   hdmi_audio_cdc ─▶ dvi_tx data islands ─▶ TV
```

A block: the walker fetches each element of the list (a node, or up to 4 nodes linked into a
lockstep group, one burst each), starts the head node's routine, serves its DMA requests, then
writes each node's words 12-15 back. After the last element the main buses go to the FIFO and
every bus is cleared. A block starts when the FIFO has room for it (1,500 blocks a second); one
has 44,992 clocks at 67.5 MHz.

## Nodes

16 words, 64-byte aligned. w0: next (word address; 0 ends the list), bit 31 linked with the next;
w1: the head's routine (control-store address), bit 31 skip; w2-w11 parameters; w12-w15 state,
written back each block (w13: the node's meter, mirrored into the monitor). The CPU writes
parameters only while the engine waits between blocks (from its block-done interrupt): a CPU
store reaches the SDRAM a byte at a time, and a node half written would be read half old.

| Routine | Parameters | State |
|---|---|---|
| `osc_saw`, `osc_tri`, `osc_pulse`, `osc_noise` | w2 step (0.32 a sample), w3 note (below), w4 volume {R, L} Q15, w5 pulse width (0-4095), w6 LFO rate << 16 (a block), w7 PWM depth (width units), w8 vibrato depth (Q15), w9-w11 envelope | w12 phase, w13 level, w14 envelope, w15 LFO phase (noise: the LFSR, start it at 1) |
| `smp` (8-bit samples) | w2 step 16.16, w3, w4, w5 the sample's word address, w6 end << 16 (frames, a multiple of 4), w7 loop length << 16 (a multiple of 4; 0 one-shot), w8 the loop start's word address, w9-w11 | w12 position 16.16 (frames, under 32768), w13, w14 |
| `sync` (a group: master, then slave) | master: w2 step, w3, w4 its volume, w5 mode (1 hard sync: the slave restarts when the master wraps; 2 ring: the slave's triangle flips with the master's top bit), w9-w11 (one envelope); slave: w18 step, w20 volume | w12 master phase, w28 slave phase |
| `flt` (state-variable filter, after the voices) | w2 cutoff F = 2 sin(pi fc / 48000) Q15, w3 damping 1/Q Q15, w4 output volume, w5 LFO rate << 16, w6 sweep depth (F units), w7 mode (0 low-pass, 1 band-pass) | w12 low, w13 F now, w14 band, w15 LFO phase |

Voices: w3 bits 7:0 a note-on count (a change starts the attack; `smp` also restarts the sample),
bit 8 the gate (off: release), bit 12 to the filter buses instead of the main mix. Envelope: w9
attack << 16 (Q15 a block), w10 {release, decay} rates (Q15 of the remaining distance a block,
toward 0 and toward the sustain), w11 sustain << 16. The level ramps across the block a sample at
a time (no 1.5 kHz zipper).

Samples: each block the `smp` routine DMAs 32 words (128 frames) from its position into the sample
buffer, and, when looping and near the end, the loop's start right after the end, so the
per-sample loop reads straight on through the loop point; it interpolates linearly.

## Micro-words and timing

72 bits: next address and branch (jump, on Z/N, the 32-sample loop, end), ALU op with two
registers or an immediate or MDR, flags, a predicate (write only if C, not C, N), a MAC op on
16-bit halves, a memory op (node read and write, sample read in four formats, sine table, bus
add/read/write), a shift or sub-op field. `SYS` ops: DMA (the sequencer waits for the burst), TICK
(the tempo tick), IRQ, ROUTE. Field list: `mc_core.v`. The routines keep three timing rules, which
`mcasm.py` checks for the first: MDR is the read issued two words before; a product reaches the
accumulator two words after its MAC; flags steer the next word.

Per output sample: an oscillator 9 words (noise 11), a sample voice 21, a sync group 15 (two
oscillators), the filter 31. Per block each node also costs its fetch, its per-block code (40-70
words) and its write-back.

## Registers (F01000)

| Address | Name | |
|---|---|---|
| +00/02 | HEAD | the list's first node (word address) |
| +04 | CTRL | bit 0 run, bits 7:4 output shift |
| +06 | TEMPO | blocks per tick (0.16), for TICK |
| +08, +0A-12 | CS_ADDR, CS_DATA0-4 | the control store: a 72-bit word, low first; CS_DATA4 commits and steps CS_ADDR |
| +14 | IRQ | pending (bit 7 block done, 0-6 SYS IRQ); write 1s to clear. The interrupt is source 3 at F00700 |
| +16 | ID | 0xA0C2 |
| +18, +1A, +1C | BLOCKS, UNDERRUNS, CLOCKS | counters; clocks the last block took |
| +1E, +20, +22 | BURSTS, WORDS, NODES | the last block's RAM bursts, words, nodes |
| +24 | MON_IDX (W) / MON_RING (R) | the monitor's index; the output ring's next position |
| +26/28 | MON_LO/HI | monitor word: 0-255 the last 256 output samples {R, L}, 256-287 the nodes' meters (w13) |

## Size and timing (GW2AR-18, the whole Tang system)

| | Fixed engine | Microcoded |
|---|---|---|
| LUT4 | 16,723 | 12,797 |
| ALU | 3,964 | 2,686 |
| LUT sites | 20,687 (99.8%) | 15,483 (75%) |
| Block RAM | 38 | 41 |
| Multipliers | 9 | 1 |

The first build failed timing (fclk 48-55 MHz against 67.5): the path ran from the node and sample
buffers through the format select into the 32-bit ALU. MDR is registered now (reads take two
words); see the Makefile's `PNR_mc` for the seed in use.

# tang

Sipeed Tang Nano 20K (Gowin GW2AR-LV18QN88C8/I7) builds, with the open
toolchain of the OSS CAD Suite (`OSS=` its `bin`, default `~/oss-cad-suite/bin`):
yosys `synth_gowin`, nextpnr-himbaechel, gowin_pack (through
`tools/gowin_pack_elvds.py` for the HDMI output: see HDMI below), openFPGALoader.

    make TOP=hdmitest bit prog    # build and load (SRAM only; the flash is never written)
    make prog                     # TOP=system is the default
    make run PROG=x.bin           # fcc image, linked at 0x1000, over the USB serial port
    make sim SIMARGS="+bytes=x.bytes +shot=f.txt +frame=7"   # a video frame, for ../fpga/tools/cmpshot.py

| `TOP` | What it is |
|---|---|
| `blinky` | bring-up: the LEDs |
| `hdmitest` | 1280x720 60 Hz on the HDMI connector: colour bars, a grey ramp, a bouncing square |
| `hdmiaudio` | colour bars with a 440 Hz (left) / 660 Hz (right) tone over HDMI audio (verified on the TV) |
| `hdmiload` | `hdmiaudio` with the SDRAM PHY under a cycling read/write load (a test bench for the HDMI link: see below) |
| `system` | the fpga_cpu: T1F, 64 KB of block RAM, the 8 MB SDRAM, docs/io.md's video and the audio engine on HDMI, the I/O registers; programs load and talk over the USB serial port |

## HDMI

`rtl/dvi_tx.v`: DVI, or (`HDMI=1`, as `system` uses it) HDMI framing with an
AVI InfoFrame once a frame (`tools/avi_packet.py`): 720p RGB, IT content,
underscan. A PLL makes 371.25 MHz from the
27 MHz crystal, a CLKDIV divides it by 5 to the 74.25 MHz pixel clock; three
pipelined TMDS encoders and the clock word go through OSER10 serializers into
the output buffers on pins 33-40. `make DEFS=-DMODE480` builds a 640x480
version for bring-up.

The output buffers are emulated differential (`ELVDS_OBUF`: two 3.3 V
outputs at 8 mA, `constraints/hdmi_elvds.cst`), as NESTang, NanoMig and
Sipeed's examples drive this board's HDMI with Gowin's tools. apicula's
gowin_pack refuses `ELVDS_OBUF` on these pins (they can also be true LVDS),
so `tools/gowin_pack_elvds.py` runs it with that check lifted and the bank
at 3.3 V; the HDMI pins' and banks' fuses then match NESTang's Gowin-built
bitstream exactly (as far as apicula decodes them). `make HDMI_OUT=tlvds`
builds the true-LVDS version (`TLVDS_OBUF`, `constraints/hdmi.cst`, plain
gowin_pack).

Why: with `TLVDS_OBUF` the FPGA's own switching noise breaks the link. The
TV checks HDMI audio packets (an error makes it mute and re-sync, blanking
the picture too), while a wrong bit in a pixel shows nothing, so the full
system lost picture and sound for seconds at a time once it played music
with audio packets on, and was clean with them off. Taken apart on the board
with a camera on the TV (`hdmiload`): the packets the system builds are
correct (captured on the board, `-DPKTCAP` below); replayed with SDRAM traffic
and no CPU (`hdmiload -DREPLAY`) the TV keeps them; 6144 registers switching
with nothing connected to HDMI (`hdmiload -DLOGICLOAD`) black it out, and
with no packets the picture drops too. With `ELVDS_OBUF` the same switching
load and the full system playing Glass Bells give a steady picture and
unbroken sound.

If a monitor shows "No Signal", reseat the HDMI cable first: on this board it
must be pushed fully into the connector. If the TV crops the edges
(overscan), its picture settings decide: on the Fire TV set used here,
"picture reset" showed the whole picture.

## Video extensions and blitter

On the Tang the video (`../fpga/rtl/video.v` with `EXT = 1`) adds what
docs/io.md lists under "Video extensions" and "Blitter": a second 8 bpp
playfield with its own scroll, a per-line scroll table for both, the sprite
buffer layer, 64 hardware sprites (up to 32x32, 16 a line), double buffering
(the buffer addresses are taken at the start of each frame; `VID_STAT` bit 0
says when) and a blitter (`rtl/blitter.v`: rectangle copy and fill with
strides and a colour key, on its own rammux port; it invalidates the CPU
cache's lines it writes). The registers are `rtl/vidx_regs.v` (F00220) and the
blitter's own (F00800). Each logical line the DMA engine fetches the
line-scroll entry, playfield A, playfield B, the sprite buffer row and the
text row, then builds the sprites' line in a line buffer; the pixel side
takes the front-most opaque layer and reads the palette once.

`progs/vidxtest.c` shows it all in one picture and checks the blitter and
the flip status (PASS on the board). Its frame from the RTL simulation
(`sim/system_tb.v +preload=`, the playfields drawn by fsim and dumped) matches
fsim's pixel for pixel.

To make room the SDRAM self-test is a build option, `make DEFS=-DSDBIST`
(serial 'C', 'D', 'E', 'B'). Utilisation with the extensions: about 16,500
LUT4s (79%) and 3,800 ALUs, 40 of 46 block RAMs; fclk 79 MHz (seed 6).

## system

The core is T1F (`../verilog/rtl/t1f_core.v`, docs/t1f.md; `make CORE=t1`
builds T1). It runs on a 67.5 MHz clock gated by a DQCE: a T1F cycle is 2
clocks (33.75 MHz) for block RAM (000000-00FFFF), 3 for I/O, and longer
while the SDRAM answers. T1F has 24-bit addresses here (`AW24`, docs/t1f.md):
all 8 MB of the SDRAM, 000000-7FFFFF, repeating every 8 MB up to the I/O at
F00000 (`progs/xramtest.c` tests 100000-7FFFFF). T1 folds its addresses to
the first 1 MB. T1F takes interrupts here (`IRQ`): the controller at F00700
with the vertical blank and the audio engine as sources (`progs/irqtest.c`,
built with `../fpga/lib/irq.c`). T1 needs 4 clocks. The SDRAM
is on the RAM machine, as on the Spartan-3: `../fpga/rtl/rammux.v` (port 0
T1 and the loader, port 1 video DMA in bursts of 8) in front of
`rtl/sdram_phy.v`; port 2 is the audio engine. Video buffers must be in the SDRAM.

Audio: `../fpga/rtl/audio_engine.v` (docs/audio.md: 32 voices, samples and
chip voices, ADSR, filter; registers at F01000 and F01800) on fclk, its
samples through `rtl/hdmi_audio_cdc.v` into `dvi_tx` (`AUDIO=1`): 48 kHz
stereo in the TV's speakers. Sample data must be in the SDRAM (the engine
reads through rammux). `progs/audtest.c` checks the engine and plays a
chord and a noise burst (`make run PROG=build/audtest.bin`).
Memory: there is no block RAM main memory; everything but I/O is the SDRAM,
behind a 32 KB direct-mapped write-through cache (16-byte lines, a one-entry
write buffer) in block RAM. T1F's bus is sampled two fclk cycles after its
edge (the paths from T1F to that logic are not timed and come out near one
cycle in this design): a cache hit or an I/O access takes 4 clocks.

The SDRAM pins drive 4 mA (`constraints/sdram.cst`) and the PHY holds the
data bus at its last value between accesses (`rtl/sdram_phy.v`): both
lowered the switching noise that upset the true-LVDS HDMI output (HDMI
above), and are kept.

Test builds (`make DEFS=...`): `-DDIAG` prints late video lines, audio FIFO
underruns and frames once a second on the console; `-DNOAUDIO` sends no
audio packets (a 48 kHz tick still drains the FIFO, so the load is the
same); `-DBARS` shows `hdmiaudio`'s colour bars in place of the picture;
`-DPKTCAP` sends no audio packets and captures the islands a copy of
`dvi_tx` would send, 8192 words at a time, out on the console as `#P` lines
(`tools/pktcap_check.py` checks them: ECC, layout, parity, spacing).
`hdmiload` options: `-DLOGICLOAD` (a 6144-register switching load, 10 s on,
10 s off), `-DNOAUDIO`, `-DREPLAY` (sends a capture's islands in place of its
own: `tools/pktcap_rom.py` makes `build/pk/replay.hex`; `sim/replay_tb.v`
simulates it).

`sim/mock/` runs `system.v` with T1 replaced by a replay of sidplay's bus
trace playing Glass Bells (from the instruction-set simulator; needs
`apps/sidplay`): the cache, rammux, PHY, video and audio engine against the
SDRAM model, with a scoreboard on every word; `+words=` dumps the HDMI words
for `tools/hdmi_audio_check.py`.

Utilisation: about 14,350 LUT4s (69%), 29 of 46 block RAMs; fclk 78.9 MHz
(placement seed 6: the margin depends on placement). The video is `../fpga/rtl/video.v`, shared with the Spartan-3, in
true colour; the I/O registers are `../fpga/rtl/iosys.v`. Programs link with
`--stack 0x10000` (the Makefile does) so the stack is in block RAM.

- `../fpga/demo/demo.c` built with `-DTANG` (`make run`, the default; it links
  with `--stack 0x1000`, the stack below the program, as it is more than the
  60 KB of block RAM above 0x1000): the demo (the earlier plasma-only demo
  measured, 60 fps: T1F 10% busy, 6.47 MIPS; T1 20% busy, 3.32 MIPS; the
  Spartan-3's T1 37% busy, 1.81 MIPS. The demo's clocks per instruction are
  in 16.875 MHz units, SDRAM waits included: T1F 2.6, T1 5.06)
- `progs/sdtest.c`: SDRAM tests, then a scrolling hi-color bitmap
- `progs/vidtest.c`: the text layer: attributes, CP437, live counters

SDRAM timing, found on the board: reads are bursts of 8 (sequential, masked
with DQM where unwanted; 270 MB/s) and writes single words. The SDRAM's clock
is fclk in phase (PLL `PSDA_SEL` "0000", `SD_PSDA` in `rtl/system.v`) and read
data is taken at the third rising edge after its slot (`cap` 3, settable
over serial). The self-test (serial 'C' 8-word bursts, 'D' 4, 'E' single,
'B' bursts after byte writes) passed with no errors in every mode on four
placements; at 180 degrees, the earlier setting, burst reads failed on some
placements (the word at the edge of the window).

The loader protocol and the console are described at the top of
`rtl/system.v`; `tools/run.py` (needs pyserial) loads an image, runs it and
prints its output until EXIT.

## Board notes

- The BL616 debugger: firmware 2025030317. If JTAG stops responding, hold S2
  while powering up.
- Serial ports: `/dev/cu.usbserial-*0` is JTAG, `*1` the FPGA's UART (pins 69 TX, 70 RX).

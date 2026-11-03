# fpga

Spartan-3 Starter Board (XC3S200-4FT256) builds. `TOP` picks the design:

| `TOP` | What it is |
|---|---|
| `system` | the fpga_cpu: the T1 core, 1 MB SRAM, the video and I/O of docs/io.md, program loading over JTAG |
| `board_test` | bring-up: seconds on the 7-segment display, a Larson scanner, a 720p text demo screen |
| `t1test` | diagnostic: T1 alone with block RAM, clocked as in `system`, running a self-check |

ISE 14.7 runs on a Linux host (`ise` in `~/.ssh/config`); the Makefile drives it
over ssh and copies the results back to `build/`:

    make TOP=system bit     # synthesize, place and route, bitstream + SVF
    make TOP=system prog    # load the FPGA (SRAM configuration: gone at power-off)
    make TOP=system run     # build fpga/demo/demo.c and run it on the board
    make TOP=system run PROG=build/x.bin    # any fcc image linked at 0x1000
    make TOP=system sim SIMARGS="+image=build/x.hex"   # the whole system in Icarus
    make scan               # read-only: chain IDs and the FPGA status register

`make prog` prints the FPGA status register: `0x18EC` has DONE high, the
design is running; `0x088C` has DONE low, the load failed.

## The system design

- `rtl/system.v`: the top. The core is T1F (`../verilog/rtl/t1f_core.v`,
  docs/t1f.md, extension X off; `make CORE=t1` builds T1) at **10 MHz**: one
  clock per five 50 MHz cycles, on a gated global clock (`rtl/clkgate.v`).
  `make S3PER=4` runs T1F at four cycles (12.5 MHz, `constraints/system_per4.ucf`):
  it meets timing and runs the demo 1.35x faster, but the demo's palette
  animation is less even than at five, for a reason not yet found. Its reset and its cycle and instruction counters
  (SR1-SR3, `CYCLE`) run on that clock.
- `rtl/memsys.v`: the SRAM, shared in periods: a CPU period (five cycles;
  T1's bus is registered at the second edge, read data reaches T1 straight
  from the pins), a video DMA period (two cycles per word, up to four words,
  taking every other period while a line is fetched) and a loader period.
  Nothing on the SRAM bus changes on T1's clock edge.
- `rtl/video.v`: docs/io.md's video at 1280x720 60 Hz: the backdrop, the
  bitmap (scrolled in x and y) and the 80x25 text layer, from SRAM through
  per-line buffers. The Spartan-3 is the low video tier (`HICOL8 = 0`): the
  bitmap is 4 bits a pixel, 8-color or 16-color direct (no palette; colors
  9-15 are 1-7 at half brightness, a checkerboard on the 3-bit VGA). The
  Tang is the high tier, with hi-color through the 256-entry palette. Not yet: the sprite layer. In simulation its frames
  equal `fsim --screenshot` pixel for pixel (`tools/cmpshot.py`).
- `rtl/audio_lite.v`: the low tier's sound (docs/io.md "Sound"): the
  3-voice device at F00400 and a streaming player (a stereo ring buffer in
  SRAM, a word a frame through rammux port 4), mixed to an I2S DAC on the A2
  connector at 48,828 Hz. `sim/progs/audtest.c` checks it: the RTL's I2S
  (`system_tb +i2s=FILE`) against `fsim --no-hicolor --wav FILE`
  (`tools/i2scmp.py`), frame for frame.
- `rtl/iosys.v`: the I/O registers (system, video, palette, board LEDs,
  switches, buttons, 7-segment display). 16-bit registers are written low
  byte then high, and read with the high byte latched from the low read.
  Not yet: keyboard, sound, files, the serial console.
- `rtl/jtagldr.v`: JTAG USER1 streams words into SRAM; USER2 holds or runs
  T1 and reads SRAM back. `tools/jtagload.py` drives it (`--status`,
  `--verify`, `--dump`).

Measured on the board with the demo: T1 1.81 MIPS while working (37% busy),
5.51 clocks per instruction (the cycle counts of docs/t1.md, plus memory
accesses lost to video DMA); T1F 1.95 MIPS, 5.11; T1F at four cycles
(S3PER=4) 2.45 MIPS, 5.08.
With T1F the design uses 97% of the LUTs.

### Why 10 MHz

T1's bus outputs (address, data, strobes) settle ~40 ns after its clock edge
(T1F's 38 ns), so they are registered two 50 MHz cycles later. With the
SRAM's 10 ns and the pins, read data reaches T1 ~25 ns after the pins (T1F:
12 ns): five cycles in all for T1, four for T1F (S3PER=4, see above). A core
that pipelines its bus (c1/c2 of docs/cores.md) is where more speed comes
from.

### Programs

`fcc` images linked at 0x1000 (the default) run unchanged in fsim and on the
board. crt0 relies on zeroed memory for the text buffer, so the loader
zeroes it (`--no-clear` to skip). `int` is 16 bits: products and offsets
that can pass 32767 need 32-bit types.

- `demo/`: the demo, six self-playing sections in a loop, all original and
  all drawn by the CPU in 16 colors (`demo.c` runs them; `gfx.c` is their
  graphics library): a platform game (`sec_platform.c`), a bullet-hell
  shooter (`sec_bullets.c`), a plasma (`sec_plasma.c`; on the Tang the
  256-color palette plasma), a third-person racer (`sec_racing.c`, 30 fps),
  an isometric flyer (`sec_iso.c`) and 30 s of a paint program on a desktop
  (`sec_desktop.c`). Row 24 shows live statistics (fps, CPU busy, thousands
  of instructions a frame, MIPS, CPI, `CYCLE`'s rate; SW0 hides it), and each
  section's latest are kept at 0x05FF00 for `jtagload.py --dump 0x05FF00 24`.
  `sectest.c` runs one section in fsim and prints its instructions a frame.
- `sim/progs/systest.c`: memory and I/O checks, output compared with fsim
- `sim/progs/vidtest.c` + `tools/mkvidtest.py`: video against fsim's renderer
- `sim/progs/hwtest.c`: on the board: an SRAM test, a hi-color bitmap, live
  frame/scan line/cycle counters
- `sim/progs/t1check.c`: the self-check `t1test` runs

## Programming

The Opella-LD is an FT2232 with Ashling's USB IDs, so OpenOCD's ftdi driver
runs it. The OpenOCD in the ESP32 Arduino package has that driver and is the
default; set `OPENOCD=` for another. openFPGALoader does not work with it (its
FT2232 profile leaves the Opella's JTAG buffers disabled).

The chain, from TDI: XC3S200 (`0x01414093`), XCF02S (`0x05045093`).

`make prog` plays iMPACT's SVF rather than OpenOCD's `pld load`: that waits
only 1 ms after JPROGRAM, before the XC3S200 has cleared its configuration
memory, and the data is ignored. iMPACT waits 14000 TCK.

JTAG user registers on this board take a scan's bits one position early: a
USER1 word n is bits 32n-1 .. 32n+30 of the scan, and a USER2 command is the
last 32 bits of a 33-bit scan. `tools/jtagload.py` allows for both; it was
found by having T1 show a loader-written word on the 7-segment display
(the readback, offset the same way, had hidden it). Reads back over JTAG
occasionally flip a bit; loads are exact.

The Platform Flash holds an Apple IIe emulator, which loads at power-up,
replacing whatever `make prog` loaded; `make prog` never writes the flash.
Writing a design there needs a bitstream with `StartUpClk:CCLK`.

## Constraints

`constraints/s3starter.ucf` has the board's pins (SRAM from Xilinx UG130);
`constraints/$(TOP).ucf` adds a design's timing, and the Makefile joins them.
For `system`, T1's clock has its own PERIOD so ISE checks hold on T1's paths
(a FROM:TO MAXDELAY gets no hold analysis), and the paths between T1 and the
50 MHz logic are multi-cycle FROM:TO constraints.

## Board notes

- VGA: red R12, green T12, blue R11, HSYNC R9, VSYNC T10; one bit per
  colour. On this board the blue series resistor has a bad solder joint
  (blue drops out until it is pressed); it needs reflowing.
- The Samsung SyncMaster 2343BWX takes 1280x720 at exactly 74.25 MHz with
  vsync on hsync's leading edge; at 75 MHz (one DCM, x3/2) the picture
  sat a few lines high and bounced.

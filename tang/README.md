# tang

Sipeed Tang Nano 20K (Gowin GW2AR-LV18QN88C8/I7) builds, with the open
toolchain of the OSS CAD Suite (`OSS=` its `bin`, default `~/oss-cad-suite/bin`):
yosys `synth_gowin`, nextpnr-himbaechel, gowin_pack, openFPGALoader.

    make TOP=hdmitest bit prog    # build and load (SRAM only; the flash is never written)
    make prog                     # TOP=system is the default
    make run PROG=x.bin           # fcc image, linked at 0x1000, over the USB serial port
    make sim SIMARGS="+bytes=x.bytes +shot=f.txt +frame=7"   # a video frame, for ../fpga/tools/cmpshot.py

| `TOP` | What it is |
|---|---|
| `blinky` | bring-up: the LEDs |
| `hdmitest` | 1280x720 60 Hz on the HDMI connector: colour bars, a grey ramp, a bouncing square |
| `system` | the fpga_cpu: T1 at 16.875 MHz, 64 KB of block RAM, docs/io.md's video on HDMI, the I/O registers; programs load and talk over the USB serial port |

## HDMI

`rtl/dvi_tx.v`: DVI, or (`HDMI=1`, as `system` uses it) HDMI framing with an
AVI InfoFrame once a frame (`tools/avi_packet.py`): 720p RGB, IT content,
underscan. A PLL makes 371.25 MHz from the
27 MHz crystal, a CLKDIV divides it by 5 to the 74.25 MHz pixel clock; three
pipelined TMDS encoders and the clock word go through OSER10 serializers into
TLVDS_OBUF pairs (pins 33-40, `constraints/hdmi.cst`). `make DEFS=-DMODE480`
builds a 640x480 version for bring-up.

If a monitor shows "No Signal", reseat the HDMI cable first: on this board it
must be pushed fully into the connector. If the TV crops the edges
(overscan), its picture settings decide: on the Fire TV set used here,
"picture reset" showed the whole picture.

## system

T1 runs on a 67.5 MHz clock gated by a DQCE: a T1 cycle is 4 clocks
(16.875 MHz) for block RAM (000000-00FFFF) and I/O, and longer while the
SDRAM answers (everything else in the 1 MB; reads take 7 clocks). The SDRAM
controller (`rtl/sdram.v`) also feeds video DMA, so video buffers must be in
the SDRAM. The video is `../fpga/rtl/video.v`, shared with the Spartan-3, in
true colour; the I/O registers are `../fpga/rtl/iosys.v`. Programs link with
`--stack 0x10000` (the Makefile does) so the stack is in block RAM.

- `../fpga/demo/demo.c` built with `-DTANG` (`make run`, the default): the demo
  (measured: 60 fps, CPU busy 20%, 3.32 MIPS, 5.06 T1 clocks per instruction,
  SDRAM waits included; the Spartan-3 runs it at 37% busy, 1.81 MIPS, CPI 5.51)
- `progs/sdtest.c`: SDRAM tests, then a scrolling hi-color bitmap
- `progs/vidtest.c`: the text layer: attributes, CP437, live counters

SDRAM timing, found on the board: its clock is fclk at 180 degrees and read
data is taken at the third rising edge after the READ. Back-to-back reads
sometimes gave the next word, so the controller leaves a cycle between
reads; capture at the falling edge, an earlier SDRAM clock, or the input
register in the I/O block (nextpnr `ioreg_in_iob`) all failed.

T1 (`../verilog/rtl/t1_core.v`) runs on a 16.875 MHz PLL clock; memory, I/O and
the UART run on the same clock shifted 180 degrees, so they see T1's bus half
a period after its edge. The loader protocol and the console are described at
the top of `rtl/system.v`; `tools/run.py` (needs pyserial) loads an image,
runs it and prints its output until EXIT.

## Board notes

- The BL616 debugger: firmware 2025030317. If JTAG stops responding, hold S2
  while powering up.
- Serial ports: `/dev/cu.usbserial-*0` is JTAG, `*1` the FPGA's UART (pins 69 TX, 70 RX).

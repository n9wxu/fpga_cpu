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

The video is `../fpga/rtl/video.v`, shared with the Spartan-3, in true
colour; the I/O registers are `../fpga/rtl/iosys.v`. Memory is four
dual-port block RAM byte lanes, so video DMA reads 32-bit words without
holding up T1. It repeats every 64 KB, so the text buffer at 0FC000 is at
C000: link with `--stack 0xC000`. The bitmap needs the SDRAM (next).
`progs/vidtest.c` is the text-layer check. In simulation its frame equals
fsim's but for the live counters.

T1 (`../verilog/rtl/t1_core.v`) runs on a 16.875 MHz PLL clock; memory, I/O and
the UART run on the same clock shifted 180 degrees, so they see T1's bus half
a period after its edge. The loader protocol and the console are described at
the top of `rtl/system.v`; `tools/run.py` (needs pyserial) loads an image,
runs it and prints its output until EXIT.

## Board notes

- The BL616 debugger: firmware 2025030317. If JTAG stops responding, hold S2
  while powering up.
- Serial ports: `/dev/cu.usbserial-*0` is JTAG, `*1` the FPGA's UART (pins 69 TX, 70 RX).

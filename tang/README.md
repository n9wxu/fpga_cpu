# tang

Sipeed Tang Nano 20K (Gowin GW2AR-LV18QN88C8/I7) builds, with the open
toolchain of the OSS CAD Suite (`OSS=` its `bin`, default `~/oss-cad-suite/bin`):
yosys `synth_gowin`, nextpnr-himbaechel, gowin_pack, openFPGALoader.

    make TOP=hdmitest bit prog    # build and load (SRAM only; the flash is never written)
    make TOP=t1tang prog
    make TOP=t1tang run PROG=x.bin   # fcc image, linked at 0x1000, over the USB serial port

| `TOP` | What it is |
|---|---|
| `blinky` | bring-up: the LEDs |
| `hdmitest` | 1280x720 60 Hz on the HDMI connector: colour bars, a grey ramp, a bouncing square |
| `t1tang` | T1 at 16.875 MHz with 64 KB of block RAM; programs load and talk over the USB serial port |

## HDMI

`rtl/dvi_tx.v`: plain DVI (no data islands). A PLL makes 371.25 MHz from the
27 MHz crystal, a CLKDIV divides it by 5 to the 74.25 MHz pixel clock; three
pipelined TMDS encoders and the clock word go through OSER10 serializers into
TLVDS_OBUF pairs (pins 33-40, `constraints/hdmi.cst`). `make DEFS=-DMODE480`
builds a 640x480 version for bring-up.

If a monitor shows "No Signal", reseat the HDMI cable first: on this board it
must be pushed fully into the connector.

## t1tang

T1 (`../verilog/rtl/t1_core.v`) on a 16.875 MHz PLL clock; memory, I/O and
the UART run on the same clock shifted 180 degrees, so they see T1's bus half
a period after its edge. The loader protocol and the console are described at
the top of `rtl/t1tang.v`; `tools/run.py` (needs pyserial) loads an image,
runs it and prints its output until EXIT.

## Board notes

- The BL616 debugger: firmware 2025030317. If JTAG stops responding, hold S2
  while powering up.
- Serial ports: `/dev/cu.usbserial-*0` is JTAG, `*1` the FPGA's UART (pins 69 TX, 70 RX).

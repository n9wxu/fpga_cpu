# fpga

Spartan-3 Starter Board (XC3S200-4FT256) build.

- `rtl/board_test.v`: bring-up test. The display counts seconds (0000-9999),
  LD0-LD7 run a Larson scanner with a fading trail, BTN0 resets, and VGA
  shows a demo screen of the fpga_cpu's text layer with the same count.
- `rtl/video720.v`: the fpga_cpu's video timing and text layer
  (docs/io.md): 1280x720 60 Hz, a 640x360 logical screen shown 2x, 80x25
  cells with the simulator's 8x14 CP437 font (`../sim/assets/font8x14.mem`)
- `rtl/clkgen.v`: the 74.25 MHz pixel clock from two DCMs in series
- `rtl/seg7.v`: seven-segment decoder
- `tools/mktext.py`: generates the demo screen (`build/text_demo.hex`)
- `sim/`: Icarus testbenches (`make sim`), with `clkgen_sim.v` standing in
  for the DCMs. The video one checks the sync timing and the 2x doubling and
  renders a frame to `build/vga_frame.png`.
- `constraints/s3starter.ucf`: pins for the clock, BTN0, switches, LEDs, the
  seven-segment display and VGA
- `scripts/ise_build.sh`: ISE 14.7 flow (xst → ngdbuild → map → par → trce →
  bitgen), then iMPACT writes the programming sequence as SVF
- `openocd/`: the Ashling Opella-LD and the board's JTAG chain

ISE runs on a Linux host (`ise` in `~/.ssh/config`); the Makefile drives it
over ssh and copies the results back to `build/`:

    make bit          # build/board_test.bit, .svf, and the logs and reports
    make prog         # load into the FPGA (SRAM: gone at power-off)
    make scan         # read-only: chain IDs and the FPGA status register
    make TOP=other    # rtl/other.v
    make sim          # the testbenches

`make prog` prints the FPGA status register: `0x18EC` has DONE high, the
design is running; `0x088C` has DONE low, the load failed.

## Programming

The Opella-LD is an FT2232 with Ashling's USB IDs, so OpenOCD's ftdi driver
runs it. The OpenOCD in the ESP32 Arduino package has that driver and is the
default; set `OPENOCD=` for another. openFPGALoader does not work with it (its
FT2232 profile leaves the Opella's JTAG buffers disabled).

The chain, from TDI: XC3S200 (`0x01414093`), XCF02S (`0x05045093`).

`make prog` plays iMPACT's SVF rather than OpenOCD's `pld load`: that waits
only 1 ms after JPROGRAM, before the XC3S200 has cleared its configuration
memory, and the data is ignored. iMPACT waits 14000 TCK.

The Platform Flash holds an Apple IIe emulator, which loads at power-up,
replacing whatever `make prog` loaded; `make prog` never writes the flash.
Writing a design there needs a bitstream with `StartUpClk:CCLK`.

## Board notes

- VGA: red R12, green T12, blue R11, HSYNC R9, VSYNC T10; one bit per
  colour. On this board the blue series resistor has a bad solder joint
  (blue drops out until it is pressed); it needs reflowing.
- The Samsung SyncMaster 2343BWX takes 1280x720 at exactly 74.25 MHz with
  vsync on hsync's leading edge; at 75 MHz (one DCM, x3/2) the picture
  sat a few lines high and bounced.

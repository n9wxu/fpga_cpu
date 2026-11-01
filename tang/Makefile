# Tang Nano 20K (GW2AR-LV18QN88C8/I7) build, with the OSS CAD Suite
# (yosys, nextpnr-himbaechel, gowin_pack, openFPGALoader).
#
#   make bit    synthesize, place and route: build/$(TOP).fs
#   make prog   load it into the FPGA (SRAM only: the flash is untouched)
#   make sim    simulate the design's testbench (Icarus)
#   make run PROG=x.bin   load an fcc image into the system over the USB serial
#               port and run it (tools/run.py)
#
# TOP picks the design: rtl/$(TOP).v plus SRC_$(TOP), constraints/tang20k.cst
# plus constraints/CST_$(TOP).
#
#   blinky     bring-up: LEDs
#   hdmitest   1280x720 60 Hz HDMI test pattern (-D MODE480: 640x480)
#   system     the fpga_cpu: T1, 64 KB of block RAM, 720p video on HDMI,
#              the I/O registers, serial loader and console

TOP    ?= system
OSS    ?= $(HOME)/oss-cad-suite/bin

DEVICE := GW2AR-LV18QN88C8/I7
FAMILY := GW2A-18C

T1 := ../verilog/rtl/t1_core.v ../verilog/rtl/t1_rf.v
SRC_blinky   :=
SRC_hdmitest := rtl/dvi_tx.v
SRC_system   := $(T1) rtl/dvi_tx.v ../fpga/rtl/video.v ../fpga/rtl/iosys.v
CST_hdmitest := constraints/hdmi.cst
CST_system   := constraints/hdmi.cst
DATA_system  := build/font8x14.mem
FREQ_blinky   := 27
FREQ_hdmitest := 74.25
FREQ_system   := 74.25

SRC := rtl/$(TOP).v $(SRC_$(TOP))
CST := constraints/tang20k.cst $(CST_$(TOP))
DEFS ?=

.PHONY: bit prog sim run clean

bit: build/$(TOP).fs

build/$(TOP).cst: $(CST)
	@mkdir -p build
	cat $^ > $@

build/font8x14.mem: ../sim/assets/font8x14.mem
	@mkdir -p build
	grep -v '^//' $< > $@

build/$(TOP).json: $(SRC) $(DATA_$(TOP))
	@mkdir -p build
	$(OSS)/yosys -q -l build/$(TOP)-yosys.log $(DEFS) -p "read_verilog -defer $(SRC); synth_gowin -top $(TOP) -json $@ -family gw2a"

build/$(TOP)_pnr.json: build/$(TOP).json build/$(TOP).cst $(wildcard constraints/$(TOP).sdc)
	$(OSS)/nextpnr-himbaechel -q -l build/$(TOP)-pnr.log --json $< --write $@ --device $(DEVICE) \
	  --vopt family=$(FAMILY) --vopt cst=build/$(TOP).cst --freq $(FREQ_$(TOP)) \
	  $(if $(wildcard constraints/$(TOP).sdc),--sdc constraints/$(TOP).sdc)
	@grep "Max frequency" build/$(TOP)-pnr.log | tail -4

build/$(TOP).fs: build/$(TOP)_pnr.json
	$(OSS)/gowin_pack -d $(FAMILY) -o $@ $< 2>/dev/null

prog: build/$(TOP).fs
	$(OSS)/openFPGALoader -b tangnano20k $<

sim: $(DATA_$(TOP))
	@mkdir -p build
	$(OSS)/iverilog -g2005 -Wall -DSIM -o build/$(TOP)_tb.vvp sim/$(TOP)_tb.v $(SRC)
	cd build && $(OSS)/vvp -n $(TOP)_tb.vvp $(SIMARGS)

PROG ?= build/vidtest.bin
# memory repeats every 64 KB: the stack goes below the text buffer (0FC000 = C000)
build/%.bin: progs/%.c
	@mkdir -p build
	cd .. && llvm/fcc -O2 --stack 0xC000 -o tang/$@ tang/$<

run: $(PROG)
	python3 tools/run.py $(PROG)

clean:
	rm -rf build

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
#   hdmiaudio  the same with a 440/660 Hz stereo tone in the TV's speakers
#   hdmiload   hdmiaudio with the SDRAM PHY under a cycling read/write load
#   system     the fpga_cpu: T1, 64 KB of block RAM, 720p video on HDMI,
#              the I/O registers, serial loader and console

TOP    ?= system
OSS    ?= $(HOME)/oss-cad-suite/bin

DEVICE := GW2AR-LV18QN88C8/I7
FAMILY := GW2A-18C

# the core: t1f (verilog/rtl/t1f_core.v, the FPGA variant; T1F periods are 2
# fclk cycles) or t1 (t1_core.v, 4)
CORE ?= t1f
T1 := ../verilog/rtl/$(CORE)_core.v ../verilog/rtl/t1_rf.v $(if $(filter t1f,$(CORE)),../verilog/rtl/t1_dec.v)
CDEF := $(if $(filter t1f,$(CORE)),-DT1F)

# HDMI output buffer: elvds (the default: emulated differential, 3.3 V 8 mA,
# as NESTang and Gowin's tools drive the Nano 20K's HDMI; constraints/
# hdmi_elvds.cst, packed by tools/gowin_pack_elvds.py since apicula refuses it
# on these pins) or tlvds (TLVDS_OBUF, LVDS25, constraints/hdmi.cst: the
# FPGA's own switching noise breaks that link, the TV dropping picture and sound)
HDMI_OUT ?= elvds
HDMI_CST := $(if $(filter elvds,$(HDMI_OUT)),constraints/hdmi_elvds.cst,constraints/hdmi.cst)
HDMI_DEF := $(if $(filter elvds,$(HDMI_OUT)),-DELVDS)
PACK     := $(if $(filter elvds,$(HDMI_OUT)),$(OSS)/tabbypy3 tools/gowin_pack_elvds.py,$(OSS)/gowin_pack)
SRC_blinky   :=
SRC_hdmitest := rtl/dvi_tx.v
SRC_hdmiaudio := rtl/dvi_tx.v rtl/hdmi_audio_cdc.v rtl/hdmi_audio_tone.v
SRC_hdmiload := rtl/dvi_tx.v rtl/hdmi_audio_cdc.v rtl/hdmi_audio_tone.v rtl/sdram_phy.v
SRC_system   := $(T1) rtl/dvi_tx.v ../fpga/rtl/video.v ../fpga/rtl/iosys.v ../fpga/rtl/rammux.v rtl/sdram_phy.v ../fpga/rtl/audio_engine.v ../fpga/rtl/audio_regs.v rtl/hdmi_audio_cdc.v rtl/vidx_regs.v rtl/blitter.v
SIM_system   := sim/sdram_model.v
CST_hdmitest := $(HDMI_CST)
CST_hdmiaudio := $(HDMI_CST)
CST_hdmiload := $(HDMI_CST) constraints/sdram.cst
CST_system   := $(HDMI_CST) constraints/sdram.cst
DATA_system  := build/font8x14.mem
FREQ_blinky   := 27
FREQ_hdmitest := 74.25
FREQ_hdmiaudio := 74.25
FREQ_hdmiload := 74.25
FREQ_system   := 74.25
# placement seed: with the cache and the audio engine, fclk (67.5 MHz) has little
# margin and depends on placement. With 23-bit video addresses (video.v AW) seeds 1-9
# gave 65.7, 64.8, 66.0, 66.0, 68.7, 55.6, 65.2, 64.9, 57.0 MHz: 5 passes
PNR_system    := --seed 5

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
	$(OSS)/yosys -q -l build/$(TOP)-yosys.log $(CDEF) $(HDMI_DEF) $(DEFS) -p "read_verilog -defer $(SRC); synth_gowin -top $(TOP) -json $@ -family gw2a"

build/$(TOP)_pnr.json: build/$(TOP).json build/$(TOP).cst $(wildcard constraints/$(TOP).sdc)
	$(OSS)/nextpnr-himbaechel -q -l build/$(TOP)-pnr.log --json $< --write $@ --device $(DEVICE) \
	  --vopt family=$(FAMILY) --vopt cst=build/$(TOP).cst --freq $(FREQ_$(TOP)) \
	  $(if $(wildcard constraints/$(TOP).sdc),--sdc constraints/$(TOP).sdc) $(PNR_$(TOP))
	@grep "Max frequency" build/$(TOP)-pnr.log | tail -4

build/$(TOP).fs: build/$(TOP)_pnr.json
	$(PACK) -d $(FAMILY) -o $@ $< 2>/dev/null

prog: build/$(TOP).fs
	$(OSS)/openFPGALoader -b tangnano20k $<

sim: $(DATA_$(TOP))
	@mkdir -p build
	$(OSS)/iverilog -g2005 -Wall -DSIM $(CDEF) -o build/$(TOP)_tb.vvp sim/$(TOP)_tb.v $(SIM_$(TOP)) $(SRC)
	cd build && $(OSS)/vvp -n $(TOP)_tb.vvp $(SIMARGS)

PROG ?= build/demo.bin
# the stack at the top of the block RAM (fast); the rest of the default layout is in SDRAM
# The demo's stack is below the program (0x0000-0x0FFF): the demo is more than the
# 60 KB of block RAM above 0x1000; its code fits there, the tail of its data is in the SDRAM.
build/demo.bin: $(wildcard ../fpga/demo/*.c ../fpga/demo/*.h)
	@mkdir -p build
	cd .. && llvm/fcc -O2 -DTANG --stack 0x1000 -o tang/$@ fpga/demo/demo.c

build/%.bin: progs/%.c
	@mkdir -p build
	cd .. && llvm/fcc -O2 --stack 0x10000 -o tang/$@ tang/$<

run: $(PROG)
	python3 tools/run.py $(PROG)

clean:
	rm -rf build

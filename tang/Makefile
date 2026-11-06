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
SRC_system   := $(T1) rtl/dvi_tx.v ../fpga/rtl/video.v ../fpga/rtl/iosys.v ../fpga/rtl/rammux.v rtl/sdram_phy.v ../fpga/rtl/audio_engine.v ../fpga/rtl/audio_regs.v rtl/hdmi_audio_cdc.v rtl/vidx_regs.v rtl/blitter.v rtl/spiflash.v \
               ../fpga/rtl/mcaudio/mc_engine.v ../fpga/rtl/mcaudio/mc_core.v ../fpga/rtl/mcaudio/mc_regs.v
SIM_system   := sim/sdram_model.v sim/spiflash_model.v
CST_hdmitest := $(HDMI_CST)
CST_hdmiaudio := $(HDMI_CST)
CST_hdmiload := $(HDMI_CST) constraints/sdram.cst
CST_system   := $(HDMI_CST) constraints/sdram.cst constraints/flash.cst
DATA_system  := build/font8x14.mem
FREQ_blinky   := 27
FREQ_hdmitest := 74.25
FREQ_hdmiaudio := 74.25
FREQ_hdmiload := 74.25
FREQ_system   := 74.25
# placement seed: the design fills ~96% of the LUT sites (LUTs and ALUs together; the write-only
# registers freed ~4%), so timing depends on the seed. With the synchronous T1F (SYNC), seeds 1-6
# give fclk 71.4, (no placement), 66.8, 71.7, 74.0, 75.3 MHz (needs 67.5; with the flash port,
# rtl/spiflash.v)
PNR_system    := --seed 6
# the flash's pins (MSPI) are I/O after configuration: the ROM (rtl/spiflash.v)
PACK_system   := --mspi_as_gpio

SRC := rtl/$(TOP).v $(SRC_$(TOP))
CST := constraints/tang20k.cst $(CST_$(TOP))
DEFS ?=

.PHONY: bit prog flash flash-rom sim run clean mcaudio prog-mc

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

# the microcoded audio engine in place of the fixed one (docs/mcaudio.md, -DMCAUDIO): the
# same system, built to build/system_mc.*. Seeds 1-6: 66.9, 65.5, 67.7, 67.7, 68.0, 69.5 MHz (needs 67.5)
PNR_mc ?= --seed 6
mcaudio: build/system_mc.fs
prog-mc: build/system_mc.fs
	$(OSS)/openFPGALoader -b tangnano20k $<
build/system_mc.json: rtl/system.v $(SRC_system) $(DATA_system)
	@mkdir -p build
	$(OSS)/yosys -q -l build/system_mc-yosys.log $(CDEF) $(HDMI_DEF) -DMCAUDIO $(DEFS) -p "read_verilog -defer rtl/system.v $(SRC_system); synth_gowin -top system -json $@ -family gw2a"
build/system_mc_pnr.json: build/system_mc.json build/system.cst
	$(OSS)/nextpnr-himbaechel -q -l build/system_mc-pnr.log --json $< --write $@ --device $(DEVICE) \
	  --vopt family=$(FAMILY) --vopt cst=build/system.cst --freq $(FREQ_system) --sdc constraints/system.sdc $(PNR_mc)
	@grep "Max frequency" build/system_mc-pnr.log | tail -4
build/system_mc.fs: build/system_mc_pnr.json
	$(PACK) -d $(FAMILY) $(PACK_system) -o $@ $< 2>/dev/null

build/$(TOP).fs: build/$(TOP)_pnr.json
	$(PACK) -d $(FAMILY) $(PACK_$(TOP)) -o $@ $< 2>/dev/null

prog: build/$(TOP).fs
	$(OSS)/openFPGALoader -b tangnano20k $<

# the bitstream into the flash (the board then configures itself at power-up), and the ROM image
# (../rom, docs/rom.md) at its offset in the flash: the copier loads it at power-up
ROM_IMG ?= ../rom/build/rom.img
ROM_OFS := 0x400000
flash: build/$(TOP).fs
	$(OSS)/openFPGALoader -b tangnano20k -f $<

flash-rom: $(ROM_IMG)
	$(OSS)/openFPGALoader -b tangnano20k -f -o $(ROM_OFS) --file-type bin $<

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

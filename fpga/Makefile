# Spartan-3 Starter Board build.
#
#   make sim    simulate the design's testbench (Icarus); SIMARGS=... are
#               passed to it
#   make bit    synthesize on the ISE host over ssh, fetch build/$(TOP).bit
#               and the SVF that programs it
#   make prog   load the bitstream into the FPGA through the Opella-LD
#               (SRAM only: lost at power-off, the Platform Flash is untouched)
#   make scan   read-only check of the JTAG chain
#   make demo   build fpga/demo/demo.c (and run: load it into the running
#               system design over JTAG)
#   make run PROG=x.bin   load any fcc image into the system design and run it
#
# TOP picks the design: rtl/$(TOP).v with sim/$(TOP)_tb.v, plus the modules
# listed in SRC_$(TOP) and the $readmemh data in DATA_$(TOP).
#
#   board_test  bring-up: counter, Larson scanner, 720p text demo screen
#   system      the fpga_cpu: T1, SRAM, video, I/O, JTAG loading
#   t1test      T1 alone with block RAM, running a self-check (diagnostic)

TOP   ?= board_test
PART  := xc3s200-4-ft256
# the board's pins, plus the design's own timing constraints if it has any
UCF   := build/$(TOP).ucf

SRC_board_test  := rtl/seg7.v rtl/clkgen.v rtl/video720.v
DATA_board_test := build/font8x14.mem build/text_demo.hex
# the core in system: t1f (verilog/rtl/t1f_core.v, docs/t1f.md) or t1
CORE ?= t1f
CDEF := $(if $(filter t1f,$(CORE)),T1F $(if $(filter 4,$(S3PER)),T1F_PER4))
SRC_system      := rtl/memsys.v rtl/iosys.v rtl/video.v rtl/jtagldr.v rtl/clkgen.v rtl/clkgate.v rtl/seg7.v \
                   ../verilog/rtl/$(CORE)_core.v ../verilog/rtl/t1_rf.v $(if $(filter t1f,$(CORE)),../verilog/rtl/t1_dec.v)
DATA_system     := build/font8x14.mem
# fcsys: the system with FC, the pipelined core (docs/fc.md)
SRC_fcsys       := rtl/fcmem.v rtl/iosys.v rtl/video.v rtl/jtagldr.v rtl/clkgen.v rtl/clkgate.v rtl/seg7.v \
                   ../verilog/rtl/fc_core.v ../verilog/rtl/t1_dec.v
DATA_fcsys      := build/font8x14.mem
# ramtest: the RAM machine (rammux.v, sram_phy.v) under mock clients (tools/ramtest.py)
SRC_ramtest     := rtl/rammux.v rtl/sram_phy.v rtl/jtagldr.v rtl/seg7.v
# ramsys: system.v's T1F, I/O and video on the RAM machine (rammux.v, sram_phy.v, cpumem.v)
SRC_ramsys      := rtl/cpumem.v rtl/rammux.v rtl/sram_phy.v rtl/iosys.v rtl/video.v rtl/jtagldr.v rtl/audio_lite.v \
                   rtl/seg7.v ../verilog/rtl/t1s_core.v ../verilog/rtl/t1s_dec.v
# ramsys fills the XC3S200: XST optimizes for area, and makes no shift registers
# of LUTs (LUTs are scarce, flip-flops are not); its timing still has slack
XSTOPT_ramsys   := -opt_mode Area -shreg_extract NO
DATA_ramsys     := build/font8x14.mem ../verilog/rtl/t1s_rom_x0.vh ../verilog/rtl/t1s_rom_x1.vh
# ramvid: the RAM machine's 1280x720 status monitor (mockcpu.v, fbvideo.v; tools/mkstatus.py)
SRC_ramvid      := rtl/rammux.v rtl/sram_phy.v rtl/fbvideo.v rtl/mockcpu.v rtl/jtagldr.v rtl/seg7.v
DATA_ramvid     := build/font8x14.mem build/stat_text.hex build/stat_fields.hex build/stat_colours.hex
SRC_t1test      := rtl/jtagldr.v rtl/clkgen.v rtl/clkgate.v rtl/seg7.v \
                   ../verilog/rtl/t1_core.v ../verilog/rtl/t1_rf.v
DATA_t1test     := build/t1test.hex

SRC   := rtl/$(TOP).v $(SRC_$(TOP))
DATA  := $(DATA_$(TOP))
# Icarus has no DCM (or BSCAN, or BUFGCE): simulate with stand-ins.
SIM_SRC := $(filter-out rtl/clkgen.v,$(SRC)) sim/clkgen_sim.v

# ISE 14.7 runs on a Linux host; the repository's files are mirrored to
# $(ISE_ROOT) there and the build happens in $(ISE_ROOT)/fpga.
ISE_HOST ?= ise
ISE_ROOT ?= fpga_cpu
ISE_DIR  := $(ISE_ROOT)/fpga
# the files to mirror, relative to the repository root
ROOT_FILES = $(foreach f,$(SRC) $(UCF) scripts/ise_build.sh $(DATA),$(if $(filter ../%,$(f)),$(patsubst ../%,%,$(f)),fpga/$(f)))

# OpenOCD with the ftdi driver. The ESP32 Arduino package ships one; any
# other build with ftdi support works too (OPENOCD=... on the command line).
OPENOCD ?= $(firstword $(wildcard $(HOME)/Library/Arduino15/packages/esp32/tools/openocd-esp32/*/bin/openocd) openocd)
OCD     := $(OPENOCD) -f openocd/opella-ld.cfg -f openocd/s3starter.cfg

FORCE:

.PHONY: sim sim-board_test sim-system sim-fcsys sim-ramtest sim-ramvid sim-ramsys sim-t1test bit prog scan demo run clean

# S3PER=4: T1F at four 50 MHz cycles a clock (system_per4.ucf); the default is five
S3PER ?= 5
DUCF := constraints/$(TOP)$(if $(and $(filter system,$(TOP)),$(filter 4,$(S3PER))),_per4).ucf
build/$(TOP).ucf: constraints/s3starter.ucf $(wildcard $(DUCF)) FORCE
	@mkdir -p build
	@cat $(filter-out FORCE,$^) > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@     # rewritten only when CORE changes it

build/font8x14.mem: ../sim/assets/font8x14.mem
	@mkdir -p build
	grep -v '^//' $< > $@

build/t1test.hex: sim/progs/t1check.c
	cd .. && llvm/fcc -O2 --stack 0x2000 -o fpga/build/t1check.bin fpga/sim/progs/t1check.c
	python3 -c "d=open('build/t1check.bin','rb').read(); d+=bytes(4096-len(d)); open('$@','w').write(''.join(f'{x:02x}\n' for x in d))"

build/text_demo.hex: tools/mktext.py
	@mkdir -p build
	python3 tools/mktext.py $@

sim: sim-$(TOP)

sim-system: $(DATA)
	iverilog -g2005 -Wall -DSIM $(addprefix -D,$(CDEF)) -o build/system_tb.vvp sim/system_tb.v sim/sram_model.v $(SIM_SRC)
	cd build && vvp -n system_tb.vvp $(SIMARGS)

sim-fcsys: $(DATA)
	iverilog -g2005 -Wall -DSIM -DFCSYS -o build/fcsys_tb.vvp sim/system_tb.v sim/sram_model.v $(SIM_SRC)
	cd build && vvp -n fcsys_tb.vvp $(SIMARGS)

sim-ramtest:
	iverilog -g2005 -DSIM -o build/ramtest_tb.vvp sim/ramtest_tb.v sim/sram_model.v $(SRC)
	cd build && vvp -n ramtest_tb.vvp $(SIMARGS)

build/stat_text.hex build/stat_fields.hex build/stat_colours.hex: tools/mkstatus.py
	python3 tools/mkstatus.py

sim-ramsys: $(DATA)
	iverilog -g2005 -Wall -DSIM -DRAMSYS -I ../verilog/rtl -o build/ramsys_tb.vvp sim/system_tb.v sim/sram_model.v $(SRC)
	cd build && vvp -n ramsys_tb.vvp $(SIMARGS)

sim-ramvid: $(DATA)
	iverilog -g2005 -DSIM -o build/ramvid_tb.vvp sim/ramvid_tb.v sim/sram_model.v $(SRC)
	cd build && vvp -n ramvid_tb.vvp $(SIMARGS)
	python3 sim/frame2png.py build/ramvid_frame.txt build/ramvid_frame.png

sim-t1test: $(DATA)
	iverilog -g2005 -Wall -DSIM -o build/t1test_tb.vvp sim/t1test_tb.v $(SIM_SRC)
	cd build && vvp -n t1test_tb.vvp

sim-board_test: $(DATA)
	iverilog -g2005 -Wall -o build/$(TOP)_tb.vvp sim/$(TOP)_tb.v $(SIM_SRC)
	cd build && vvp -n $(TOP)_tb.vvp
	iverilog -g2005 -Wall -o build/video720_tb.vvp sim/video720_tb.v rtl/video720.v
	cd build && vvp -n video720_tb.vvp
	python3 sim/frame2png.py --doubled build/vga_frame.txt build/vga_frame.png

bit: build/$(TOP).bit

build/$(TOP).bit: $(SRC) $(UCF) scripts/ise_build.sh $(DATA)
	ssh $(ISE_HOST) mkdir -p $(ISE_DIR)/build
	cd .. && rsync -aR $(ROOT_FILES) $(ISE_HOST):$(ISE_ROOT)/
	ssh $(ISE_HOST) 'cd $(ISE_DIR) && XSTDEF="$(if $(filter system,$(TOP)),$(CDEF))" XSTOPT="$(XSTOPT_$(TOP))" scripts/ise_build.sh $(TOP) $(PART) $(UCF) $(SRC) > build/$(TOP).log 2>&1' \
	  || { ssh $(ISE_HOST) 'tail -40 $(ISE_DIR)/build/$(TOP).log'; exit 1; }
	scp -q $(ISE_HOST):$(ISE_DIR)/build/$(TOP).{log,syr,twr,bit,svf} build/
	@grep -A3 "Timing summary" build/$(TOP).twr

# Plays iMPACT's sequence (see scripts/ise_build.sh). Status 0x18EC: DONE high.
prog: build/$(TOP).bit
	$(OCD) -c init -c "svf -quiet build/$(TOP).svf" -c "virtex2 read_stat xc3s.pld" -c shutdown

# Programs for TOP=system: fcc images loaded over JTAG (tools/jtagload.py)
PROG ?= build/demo.bin
build/demo.bin: $(wildcard demo/*.c demo/*.h) FORCE
	@mkdir -p build
	cd .. && llvm/fcc -O2 --stack 0x20000 -o fpga/$@ fpga/demo/demo.c

demo: build/demo.bin

run: $(PROG)
	python3 tools/jtagload.py $(PROG)

scan:
	$(OCD) -c init -c scan_chain -c "virtex2 read_stat xc3s.pld" -c shutdown

clean:
	rm -rf build

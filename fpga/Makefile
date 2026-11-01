# Spartan-3 Starter Board build.
#
#   make sim    simulate the design's testbench (Icarus)
#   make bit    synthesize on the ISE host over ssh, fetch build/$(TOP).bit
#               and the SVF that programs it
#   make prog   load the bitstream into the FPGA through the Opella-LD
#               (SRAM only: lost at power-off, the Platform Flash is untouched)
#   make scan   read-only check of the JTAG chain
#
# TOP picks the design: rtl/$(TOP).v with sim/$(TOP)_tb.v, plus the modules
# listed in SRC_$(TOP).

TOP   ?= board_test
PART  := xc3s200-4-ft256
UCF   := constraints/s3starter.ucf
SRC_board_test := rtl/seg7.v rtl/clkgen.v rtl/video720.v
SRC   := rtl/$(TOP).v $(SRC_$(TOP))
# Icarus has no DCM: simulate with a behavioural stand-in.
SIM_SRC := $(filter-out rtl/clkgen.v,$(SRC)) sim/clkgen_sim.v

# Data the RTL loads with $readmemh, by file name, from the directory the
# tools run in (build/, locally and on the ISE host).
DATA := build/font8x14.mem build/text_demo.hex

# ISE 14.7 runs on a Linux host; the build happens in $(ISE_DIR) there.
ISE_HOST ?= ise
ISE_DIR  ?= fpga_cpu/fpga

# OpenOCD with the ftdi driver. The ESP32 Arduino package ships one; any
# other build with ftdi support works too (OPENOCD=... on the command line).
OPENOCD ?= $(firstword $(wildcard $(HOME)/Library/Arduino15/packages/esp32/tools/openocd-esp32/*/bin/openocd) openocd)
OCD     := $(OPENOCD) -f openocd/opella-ld.cfg -f openocd/s3starter.cfg

.PHONY: sim bit prog scan clean

build/font8x14.mem: ../sim/assets/font8x14.mem
	@mkdir -p build
	grep -v '^//' $< > $@

build/text_demo.hex: tools/mktext.py
	@mkdir -p build
	python3 tools/mktext.py $@

sim: $(DATA)
	iverilog -g2005 -Wall -o build/$(TOP)_tb.vvp sim/$(TOP)_tb.v $(SIM_SRC)
	cd build && vvp -n $(TOP)_tb.vvp
	iverilog -g2005 -Wall -o build/video720_tb.vvp sim/video720_tb.v rtl/video720.v
	cd build && vvp -n video720_tb.vvp
	python3 sim/frame2png.py --doubled build/vga_frame.txt build/vga_frame.png

bit: build/$(TOP).bit

build/$(TOP).bit: $(SRC) $(UCF) scripts/ise_build.sh $(DATA)
	ssh $(ISE_HOST) mkdir -p $(ISE_DIR)/build
	rsync -a --relative $(SRC) $(UCF) scripts/ise_build.sh $(DATA) $(ISE_HOST):$(ISE_DIR)/
	ssh $(ISE_HOST) 'cd $(ISE_DIR) && scripts/ise_build.sh $(TOP) $(PART) $(UCF) $(SRC) > build/$(TOP).log 2>&1' \
	  || { ssh $(ISE_HOST) 'tail -40 $(ISE_DIR)/build/$(TOP).log'; exit 1; }
	scp -q $(ISE_HOST):$(ISE_DIR)/build/$(TOP).{log,syr,twr,bit,svf} build/
	@grep -A3 "Timing summary" build/$(TOP).twr

# Plays iMPACT's sequence (see scripts/ise_build.sh). Status 0x18EC: DONE high.
prog: build/$(TOP).bit
	$(OCD) -c init -c "svf -quiet build/$(TOP).svf" -c "virtex2 read_stat xc3s.pld" -c shutdown

scan:
	$(OCD) -c init -c scan_chain -c "virtex2 read_stat xc3s.pld" -c shutdown

clean:
	rm -rf build

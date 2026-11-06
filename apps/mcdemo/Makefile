# mcdemo: the microcoded audio engine, feature by feature, with a live dataflow diagram
#   make           build/mcdemo.bin
#   make run       load it on the Tang (needs the -DMCAUDIO bitstream: cd ../../tang && make prog-mc)
#   make viewtest  the screen alone, with made-up data, in fsim (build/viewtest.ppm)
ROOT  := $(abspath ../..)
FCC   ?= $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
BUILD := build
INC   := -I $(BUILD) -I src -I $(ROOT)/fpga/lib
SRC   := src/demo.c src/mc.c src/view.c $(ROOT)/fpga/lib/irq.c

all: $(BUILD)/mcdemo.bin

$(BUILD)/mc_cs.h: $(ROOT)/fpga/tools/mcasm.py
	@mkdir -p $(BUILD)
	python3 $< --c $@

$(BUILD)/samples.h: tools/samples.py
	@mkdir -p $(BUILD)
	python3 $< $@

$(BUILD)/mcdemo.bin: $(SRC) $(wildcard src/*.h) $(BUILD)/mc_cs.h $(BUILD)/samples.h
	$(FCC) -O2 $(INC) $(SRC) -o $@

$(BUILD)/viewtest.bin: src/viewtest.c src/view.c src/view.h
	@mkdir -p $(BUILD)
	$(FCC) -O2 -DFRAMES=200 $(INC) src/viewtest.c src/view.c -o $@

viewtest: $(BUILD)/viewtest.bin
	$(FSIM) -q --xram 7 --true-color --screenshot $(BUILD)/viewtest.ppm $<

run: $(BUILD)/mcdemo.bin
	python3 $(ROOT)/tang/tools/run.py $< -i

clean:
	rm -rf $(BUILD)

.PHONY: all run viewtest clean

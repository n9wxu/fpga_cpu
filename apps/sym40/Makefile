# sym40: the stress test (Mozart, Symphony No. 40, I) on the microcoded engine, with a visualizer
#   make          build/sym40.bin            make run: load it (needs cd ../../tang && make prog-mc)
#   make VIZ=src/viz_stub.c                  the music with a text stand-in for the visualizer
ROOT  := $(abspath ../..)
FCC   ?= $(ROOT)/llvm/fcc
BUILD := build
VIZ   ?= src/viz.c
INC   := -I $(BUILD) -I src -I ../mcdemo/src -I ../mcdemo/build -I $(ROOT)/fpga/lib
SRC   := src/sym40.c ../mcdemo/src/mc.c $(VIZ) $(ROOT)/fpga/lib/irq.c

all: $(BUILD)/sym40.bin

$(BUILD)/instruments.h: tools/instruments.py
	@mkdir -p $(BUILD)
	python3 $< $@

$(BUILD)/score.h: tools/score.py
	@mkdir -p $(BUILD)
	python3 $< $@

../mcdemo/build/mc_cs.h: $(ROOT)/fpga/tools/mcasm.py
	$(MAKE) -C ../mcdemo build/mc_cs.h

$(BUILD)/sym40.bin: $(SRC) $(wildcard src/*.h) $(BUILD)/score.h $(BUILD)/instruments.h ../mcdemo/build/mc_cs.h
	$(FCC) -O2 $(INC) $(SRC) -o $@

run: $(BUILD)/sym40.bin
	python3 $(ROOT)/tang/tools/run.py $< -i

clean:
	rm -rf $(BUILD)

.PHONY: all run clean

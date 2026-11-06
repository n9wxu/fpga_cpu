# toccata: Bach's Toccata and Fugue in D minor on a 64-node pipe organ (the microcoded engine),
# under sym40's falling-notes visualizer
#   make          build/toccata.bin          make run: load it (needs cd ../../tang && make prog-mc)
ROOT  := $(abspath ../..)
FCC   ?= $(ROOT)/llvm/fcc
BUILD := build
VIZ   ?= ../sym40/src/viz.c
INC   := -I $(BUILD) -I src -I ../sym40/src -I ../mcdemo/src -I ../mcdemo/build -I $(ROOT)/fpga/lib
SRC   := src/toccata.c ../mcdemo/src/mc.c $(VIZ) $(ROOT)/fpga/lib/irq.c

all: $(BUILD)/toccata.bin

$(BUILD)/pipes.h: tools/pipes.py
	@mkdir -p $(BUILD)
	python3 $< $@

$(BUILD)/score.h: tools/score.py data/ToccataFugue.mid
	@mkdir -p $(BUILD)
	python3 $^ $@

../mcdemo/build/mc_cs.h: $(ROOT)/fpga/tools/mcasm.py
	$(MAKE) -C ../mcdemo build/mc_cs.h

$(BUILD)/toccata.bin: $(SRC) ../sym40/src/viz.h ../mcdemo/src/mc.h $(BUILD)/score.h $(BUILD)/pipes.h ../mcdemo/build/mc_cs.h
	$(FCC) -O2 $(INC) $(SRC) -o $@

run: $(BUILD)/toccata.bin
	python3 $(ROOT)/tang/tools/run.py $< -i

clean:
	rm -rf $(BUILD)

.PHONY: all run clean

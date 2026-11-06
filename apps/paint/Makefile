# paint: an 8-colour paint program for the Spartan-3 (see README.md).
#   make              build build/paint.bin (needs llvm/fcc)
#   make run          load it into the ramsys design over JTAG and run it
#   make fsim         run it in fsim's GUI, as the Spartan-3 (the low video tier)
#   make test         scripted sessions in fsim: out/*.png (tools/fsimtest.py)
ROOT  := $(abspath ../..)
FCC   := $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
PY    := python3
SRCS  := src/main.c src/draw.c src/view.c src/win.c src/ui.c $(ROOT)/fpga/lib/ps2.c $(ROOT)/fpga/lib/irq.c
HDRS  := $(wildcard src/*.h) $(ROOT)/fpga/lib/ps2.h $(ROOT)/fpga/lib/irq.h
# the program, its data and stack below 0x01A000 (src/paint.h's memory map)
STACK := 0x01A000

all: build/paint.bin

build/paint.bin: $(SRCS) $(HDRS)
	@mkdir -p build
	$(FCC) -O2 --stack $(STACK) -Isrc -o $@ $(SRCS)
	@$(PY) tools/checksize.py build/paint.sym $(STACK)

build/paint_test.bin: $(SRCS) $(HDRS)
	@mkdir -p build
	$(FCC) -O2 -DNOACCEL $(if $(PERF),-DPERF) --stack $(STACK) -Isrc -o $@ $(SRCS)
	@$(PY) tools/checksize.py build/paint_test.sym $(STACK)

run: build/paint.bin
	$(PY) $(ROOT)/fpga/tools/jtagload.py build/paint.bin

fsim: build/paint.bin
	$(ROOT)/sim/build/fsim-gui --no-hicolor build/paint.bin

test: build/paint_test.bin
	$(PY) tools/fsimtest.py

.PHONY: all run fsim test

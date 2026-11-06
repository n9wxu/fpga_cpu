# The Future: a ten-minute demo for the Tang Nano 20K (see README.md).
#   make                     build/future.bin
#   make run                 load and run it on the Tang (tang/tools/run.py)
#   make shot START=p AT=s   fsim from phrase p, stopped at 16th s of the song: out/p<p>_<s>.png
#   make assets              regenerate src/assets.h (tools/mkassets.py)
ROOT  := $(abspath ../..)
FCC   ?= $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
PY    := python3
INC   := -Isrc -I../demo80s/src -I$(ROOT)/fpga/demo
SRCS  := src/main.c
HDRS  := $(wildcard src/*.h src/*.c) ../demo80s/src/hw.h
START ?= 0
AT    ?= 64
DEFS  ?=

all: build/future.bin

src/assets.h: tools/mkassets.py
	$(PY) tools/mkassets.py src/assets.h
assets: src/assets.h

build/future.bin: $(SRCS) $(HDRS) src/assets.h
	@mkdir -p build
	$(FCC) -O2 --stack 0x40000 $(INC) $(DEFS) -o $@ $(SRCS)
	@$(PY) -c "import os,sys; e=0x1000+os.path.getsize('$@')+0x2000; sys.exit('program too big: %#x' % e) if e > 0x40000-0x3000 else None"

shot: $(SRCS) $(HDRS) src/assets.h
	@mkdir -p build out
	$(FCC) -O2 --stack 0x40000 $(INC) $(DEFS) -DSTART=$(START) -DSTOP_AT=$(AT) -o build/shot.bin $(SRCS)
	$(FSIM) -q --true-color --max-cycles 40000000000 --screenshot out/p$(START)_$(AT).ppm build/shot.bin </dev/null
	@sips -s format png out/p$(START)_$(AT).ppm --out out/p$(START)_$(AT).png >/dev/null 2>&1; rm -f out/p$(START)_$(AT).ppm

run: build/future.bin
	$(PY) ../../tang/tools/run.py build/future.bin

.PHONY: all assets shot run

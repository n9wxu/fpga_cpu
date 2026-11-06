# demo80s: the 1980s home computers on the Tang Nano 20K, then the Tang pulling ahead (see README.md).
#   make                     build/demo80s.bin
#   make run                 load and run it on the Tang (tang/tools/run.py)
#   make shot ONLY=n AT=t    section n in fsim, stopped t 50 Hz ticks in: out/s<n>_<t>.png
#   make music               regenerate the modules (tools/mkmusic.py -> src/music_mods.h)
ROOT  := $(abspath ../..)
FCC   ?= $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
PY    := python3
INC   := -Isrc -I../modplay/src -I$(ROOT)/fpga/demo
SRCS  := src/main.c ../modplay/src/mod.c
HDRS  := $(wildcard src/*.h src/*.c) ../modplay/src/mod.h
ONLY  ?= 1
AT    ?= 400

all: build/demo80s.bin

src/music_mods.h: tools/mkmusic.py
	$(PY) tools/mkmusic.py build
music: src/music_mods.h

build/demo80s.bin: $(SRCS) $(HDRS) src/music_mods.h
	@mkdir -p build
	$(FCC) -O2 --stack 0x40000 $(INC) -o $@ $(SRCS)
	@# 6 KB of stack below 0x40000 (the finale's sprite buffer is at 0x100000, src/sec_finale.c)
	@$(PY) -c "import os,sys; e=0x1000+os.path.getsize('$@')+0x2000; sys.exit('program too big: %#x' % e) if e > 0x40000-0x1800 else None"

shot: $(SRCS) $(HDRS) src/music_mods.h
	@mkdir -p build out
	$(FCC) -O2 --stack 0x40000 $(INC) -DONLY=$(ONLY) -DSTOP_AT=$(AT) -o build/shot.bin $(SRCS)
	$(FSIM) -q --true-color --max-cycles 20000000000 --screenshot out/s$(ONLY)_$(AT).ppm build/shot.bin </dev/null
	@sips -s format png out/s$(ONLY)_$(AT).ppm --out out/s$(ONLY)_$(AT).png >/dev/null 2>&1; rm -f out/s$(ONLY)_$(AT).ppm

run: build/demo80s.bin
	$(PY) ../../tang/tools/run.py build/demo80s.bin

.PHONY: all music shot run

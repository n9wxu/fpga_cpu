# modplay: an Amiga ProTracker MOD player and tracker display for the Tang Nano 20K (see README.md).
#   make              build build/modplay.bin
#   make run MODS="a.mod b.mod"   load the image and the MODs over the serial port and run (tang/tools/run.py)
#   make fsim MODS=a.mod          FSIM_TICKS ticks in fsim, as fast as it goes: out/screen.png
ROOT  := $(abspath ../..)
FCC   := $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
PY    := python3
SRCS  := src/main.c src/mod.c src/ui.c
HDRS  := $(wildcard src/*.h)
FSIM_TICKS ?= 300
MODS  ?=

all: build/modplay.bin

src/tables.h: tools/gentables.py
	$(PY) tools/gentables.py

build/modplay.bin: $(SRCS) $(HDRS) src/tables.h
	@mkdir -p build
	$(FCC) -O2 --stack 0x30000 -Isrc -o $@ $(SRCS)

build/modplay_test.bin: $(SRCS) $(HDRS) src/tables.h
	@mkdir -p build
	$(FCC) -O2 -DNOWAIT -DMAXTICKS=$(FSIM_TICKS) --stack 0x30000 -Isrc -o $@ $(SRCS)

fsim: build/modplay_test.bin
	@test -n "$(MODS)" || (echo 'MODS="a.mod ..." needed'; exit 1)
	@mkdir -p out
	$(PY) tools/pack.py image build/modplay_test.bin build/fsim_image.bin $(MODS)
	$(FSIM) -q --true-color --max-cycles 4000000000 --screenshot out/screen.ppm build/fsim_image.bin </dev/null
	@sips -s format png out/screen.ppm --out out/screen.png >/dev/null 2>&1 || true

run: build/modplay.bin
	@test -n "$(MODS)" || (echo 'MODS="a.mod ..." needed'; exit 1)
	$(PY) ../../tang/tools/run.py -i build/modplay.bin $$($(PY) tools/pack.py args $(MODS))

.PHONY: all fsim run

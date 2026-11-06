# sidplay: a PSID player and tracker display for the Tang Nano 20K (see README.md).
#   make              build build/sidplay.bin
#   make fsim         play for FSIM_TICKS frames in fsim: out/screen.png, engine register log, per-frame statistics
#   make run          load the image and the tunes into the board over the serial port, run (TUNE=file.sid for your own)
#   make host         host build of the player core (cc): writes engine scripts for the audio model
#   make audio        render every tune with the engine model (fpga/tools/audio_ref.py) and with a SID reference -> out/*.wav
#   make tunes        rebuild tunes/*.sid (tools/mktunes.py)
ROOT  := $(abspath ../..)
FCC   := $(ROOT)/llvm/fcc
FSIM  := $(ROOT)/sim/build/fsim
PY    := python3
S6    := ../sid6502/src
TUNES := tunes/neondrift.sid tunes/glassbells.sid tunes/syncrider.sid
SRCS  := src/main.c src/ui.c src/model.c src/player.c src/sidmap.c src/psid.c src/hw_target.c \
         $(S6)/interp.c $(S6)/jit.c $(S6)/jit_rt.s
HDRS  := $(wildcard src/*.h) $(S6)/sid6502.h
CODE_WORDS ?= 20000
# All memory is SDRAM behind a cache (src/layout.h): the 6502's RAM and attributes, the SID log and the JIT's tables
# at fixed addresses; the translation cache (CODE_WORDS 16-bit words) is BSS after the code.
DEFS  := -DSID_MEM_ADDR=0x040000 -DSID_ATTR_ADDR=0x054000 -DSID_LOG_ADDR=0x050000 -DSID_LOG_MAX=1024 \
         -DJIT_DATA_ADDR=0x066000 -DMAX_EV=1200 -DHSIZE=2048 -DCODE_WORDS=$(CODE_WORDS)
FSIM_TICKS ?= 400
SIZECHECK := $(PY) tools/sizecheck.py

all: build/sidplay.bin

src/tables.h: tools/gentables.py
	$(PY) tools/gentables.py
$(TUNES): tools/mktunes.py ../sid6502/tools/m6502.py
	$(PY) tools/mktunes.py tunes
tunes: $(TUNES)
src/tune0.h: tunes/neondrift.sid tools/bin2c.py
	$(PY) tools/bin2c.py tunes/neondrift.sid src/tune0.h tune0

build/sidplay.bin: $(SRCS) $(HDRS) src/tables.h src/tune0.h tools/sizecheck.py
	@mkdir -p build
	$(FCC) -O2 $(DEFS) --stack 0x30000 -I$(S6) -Isrc -o $@ $(SRCS) 2>&1 | $(SIZECHECK)

# the test build: no real-time wait, a fixed number of frames, statistics and the engine log
build/sidplay_test.bin: $(SRCS) $(HDRS) src/tables.h src/tune0.h tools/sizecheck.py
	@mkdir -p build
	$(FCC) -O2 $(DEFS) -DNOWAIT -DSTATS -DAUDLOG -DMAXTICKS=$(FSIM_TICKS) --stack 0x30000 -I$(S6) -Isrc -o $@ $(SRCS) 2>&1 | $(SIZECHECK)

build/fsim_image.bin: build/sidplay_test.bin $(TUNES) tools/mkimage.py
	$(PY) tools/mkimage.py build/sidplay_test.bin $@ $(TUNES)

# stdin must not be a terminal: the console input is polled (a key would change the song)
fsim: build/fsim_image.bin
	@mkdir -p out
	$(FSIM) -q --true-color --max-cycles 4000000000 --screenshot out/screen.ppm \
	  --dump 0x035000:41000:out/dump.bin $(FSIMEXTRA) build/fsim_image.bin </dev/null
	$(PY) tools/stats.py out/dump.bin
	$(PY) tools/log2script.py out/dump.bin out/fsim.script
	@sips -s format png out/screen.ppm --out out/screen.png >/dev/null 2>&1 || true

run: build/sidplay.bin $(TUNES)
	$(PY) ../../tang/tools/run.py -i build/sidplay.bin $(if $(TUNE),--tune 0x080000:$(TUNE) --zero 0x090000:4 --zero 0x0A0000:4 --zero 0x0B0000:4,\
	  --tune 0x080000:tunes/neondrift.sid --tune 0x090000:tunes/glassbells.sid --tune 0x0A0000:tunes/syncrider.sid --zero 0x0B0000:4)

build/sidplay_host: $(wildcard src/*.c) $(HDRS) src/tables.h
	@mkdir -p build
	cc -O2 -DHOST -I$(S6) -Isrc -o $@ src/hw_host.c src/player.c src/sidmap.c src/model.c src/psid.c $(S6)/interp.c $(S6)/stub_host.c

host: build/sidplay_host

AUDIO_SECS ?= 30
audio: build/sidplay_host
	@mkdir -p out
	for t in neondrift glassbells syncrider; do \
	  build/sidplay_host tunes/$$t.sid 1 $(AUDIO_SECS) out/$$t.script out/$$t.siddump && \
	  $(PY) tools/audiorender.py out/$$t.script out/$$t.wav && \
	  $(PY) tools/sidref.py out/$$t.siddump out/$$t-ref.wav $(AUDIO_SECS) ; done

.PHONY: all tunes fsim run host audio

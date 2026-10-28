# fpga_cpu BASIC and Doom in the browser: the simulator core compiled to WebAssembly
#   make         build build/{index.html,fsim.js,fsim.wasm,fsim.data} and Doom's
#                build/{doom.A.bin,doom.B.bin,doom.A.sym,doom.B.sym,DOOM.WAD.gz,FREEDOOM-COPYING.txt}
#   make serve   build, then serve on http://localhost:8000
#   make deploy  build, then rsync to $(DEPLOY), e.g. user@host:/srv/www/basic/
#                (set DEPLOY on the command line or in web/local.mk, which git ignores)
# Needs emcc (Emscripten), the BASIC images (make -C ../basic target) and,
# for Doom, the LLVM toolchain and network access for Freedoom (get_wad.sh).

ROOT   := ..
SIM    := $(ROOT)/sim
BASIC  := $(ROOT)/basic
BUILD  := build
EMCC   ?= emcc
-include local.mk

CORE   := $(SIM)/src/cpu.c $(SIM)/src/dev.c $(SIM)/src/video.c $(SIM)/src/front.c $(SIM)/src/dbg.c $(SIM)/src/font8x14.c \
          $(SIM)/src/gen/isa_gen.c $(SIM)/src/gen/exec_gen.c
ROMS   := $(BASIC)/build/basic.A.bin $(BASIC)/build/basic.B.bin
SYMS   := $(ROMS:.bin=.sym)
DOOM   := $(ROOT)/benchmarks/doom
DOOMBINS := $(DOOM)/build/doom.A.bin $(DOOM)/build/doom.B.bin
# The site publishes only Freedoom (BSD), never a shareware DOOM1.WAD.
WAD    := $(DOOM)/wad/DOOM.WAD
EXAMPLES := $(wildcard $(BASIC)/examples/*.BAS)

EMFLAGS := -O3 -std=gnu11 -Wall -Wno-unused-parameter -I$(SIM)/src \
           -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sENVIRONMENT=web -lidbfs.js \
           -sEXPORTED_FUNCTIONS=_main,_malloc \
           -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ccall,UTF8ToString,HEAPU8,HEAPF32 \
           $(foreach r,$(ROMS) $(SYMS),--preload-file $(r)@/rom/$(notdir $(r))) \
           --preload-file $(BASIC)/examples@/examples

all: $(BUILD)/fsim.js $(BUILD)/index.html doom

# Doom is fetched by the page when picked, not preloaded
doom: $(BUILD)/doom.A.bin $(BUILD)/doom.B.bin $(BUILD)/doom.A.sym $(BUILD)/doom.B.sym $(BUILD)/DOOM.WAD.gz $(BUILD)/FREEDOOM-COPYING.txt

$(DOOMBINS):
	$(MAKE) -C $(DOOM)

$(BUILD)/doom.%.bin: $(DOOM)/build/doom.%.bin
	@mkdir -p $(BUILD)
	cp $< $@

# symbols for the debugger panels (fcc writes them beside the image)
$(BUILD)/doom.%.sym: $(DOOM)/build/doom.%.bin
	@mkdir -p $(BUILD)
	cp $(DOOM)/build/doom.$*.sym $@

$(WAD):
	FREEDOOM=1 $(DOOM)/get_wad.sh

$(BUILD)/DOOM.WAD.gz: $(WAD)
	@mkdir -p $(BUILD)
	gzip -9 -n -c $< > $@

$(BUILD)/FREEDOOM-COPYING.txt: $(WAD)
	@mkdir -p $(BUILD)
	cp $(DOOM)/wad/FREEDOOM-COPYING.txt $@

$(SIM)/build/gen.stamp: $(ROOT)/cpu/isa.yaml
	$(MAKE) -C $(SIM) build/gen.stamp

$(ROMS) $(SYMS):
	$(MAKE) -C $(BASIC) target

$(BUILD)/fsim.js: web.c $(CORE) $(SIM)/build/gen.stamp $(SIM)/src/cpu.h $(SIM)/src/dev.h $(SIM)/src/front.h $(SIM)/src/dbg.h $(ROMS) $(SYMS) $(EXAMPLES)
	@mkdir -p $(BUILD)
	$(EMCC) $(EMFLAGS) -o $@ web.c $(CORE)

$(BUILD)/index.html: index.html
	@mkdir -p $(BUILD)
	cp $< $@

serve: all
	cd $(BUILD) && python3 -m http.server 8000

deploy: all
	@test -n "$(DEPLOY)" || { echo "set DEPLOY=user@host:dir (or put it in web/local.mk)"; exit 1; }
	rsync -rlt --delete --chmod=D755,F644 $(BUILD)/ $(DEPLOY)

clean:
	rm -rf $(BUILD)

.PHONY: all doom serve deploy clean

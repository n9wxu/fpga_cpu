# fpga_cpu BASIC in the browser: the simulator core compiled to WebAssembly
#   make         build build/{index.html,fsim.js,fsim.wasm,fsim.data}
#   make serve   build, then serve on http://localhost:8000
# Needs emcc (Emscripten) and the BASIC images (make -C ../basic target).

ROOT   := ..
SIM    := $(ROOT)/sim
BASIC  := $(ROOT)/basic
BUILD  := build
EMCC   ?= emcc

CORE   := $(SIM)/src/cpu.c $(SIM)/src/dev.c $(SIM)/src/video.c $(SIM)/src/front.c $(SIM)/src/font8x14.c \
          $(SIM)/src/gen/isa_gen.c $(SIM)/src/gen/exec_gen.c
ROMS   := $(BASIC)/build/basic.A.bin $(BASIC)/build/basic.B.bin
EXAMPLES := $(wildcard $(BASIC)/examples/*.BAS)

EMFLAGS := -O3 -std=gnu11 -Wall -Wno-unused-parameter -I$(SIM)/src \
           -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sENVIRONMENT=web -lidbfs.js \
           -sEXPORTED_FUNCTIONS=_main,_malloc \
           -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ccall,UTF8ToString,HEAPU8,HEAPF32 \
           $(foreach r,$(ROMS),--preload-file $(r)@/rom/$(notdir $(r))) \
           --preload-file $(BASIC)/examples@/examples

all: $(BUILD)/fsim.js $(BUILD)/index.html

$(SIM)/build/gen.stamp: $(ROOT)/cpu/isa.yaml
	$(MAKE) -C $(SIM) build/gen.stamp

$(ROMS) &:
	$(MAKE) -C $(BASIC) target

$(BUILD)/fsim.js: web.c $(CORE) $(SIM)/build/gen.stamp $(SIM)/src/cpu.h $(SIM)/src/dev.h $(SIM)/src/front.h $(ROMS) $(EXAMPLES)
	@mkdir -p $(BUILD)
	$(EMCC) $(EMFLAGS) -o $@ web.c $(CORE)

$(BUILD)/index.html: index.html
	@mkdir -p $(BUILD)
	cp $< $@

serve: all
	cd $(BUILD) && python3 -m http.server 8000

clean:
	rm -rf $(BUILD)

.PHONY: all serve clean

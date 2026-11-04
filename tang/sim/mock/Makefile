# RAM-machine mock: the Tang system (real cache/rammux/PHY/video/audio engine) fed the bus
# accesses of sidplay playing Glass Bells instead of T1's. See README at the top of mock_tb.v.
#   make            trace + build + run (FRAMES video frames, default 10)
#   make trace      fsim log -> build/gb.trace, build/gb.hex      make run IDLE=1 FRAMES=20
ROOT   := ../../..
OSS    ?= $(HOME)/oss-cad-suite/bin
FSIM   := $(ROOT)/sim/build/fsim
SIDDIR := $(ROOT)/apps/sidplay
FRAMES ?= 10
IDLE   ?= 0
TICK0  ?= 8
TICKS  ?= 12
RTL    := $(ROOT)/tang/rtl/system.v $(ROOT)/tang/rtl/dvi_tx.v $(ROOT)/tang/rtl/sdram_phy.v $(ROOT)/tang/rtl/hdmi_audio_cdc.v \
          $(ROOT)/fpga/rtl/video.v $(ROOT)/fpga/rtl/iosys.v $(ROOT)/fpga/rtl/rammux.v $(ROOT)/fpga/rtl/audio_engine.v \
          $(ROOT)/fpga/rtl/audio_regs.v $(ROOT)/tang/sim/sdram_model.v

all: run
build/trace2mock: trace2mock.c
	@mkdir -p build; cc -O2 -o $@ $<
build/gb_image.bin: $(SIDDIR)/build/sidplay_test.bin
	@mkdir -p build; python3 $(SIDDIR)/tools/mkimage.py $< $@ $(SIDDIR)/tunes/glassbells.sid
build/gb.log: build/gb_image.bin
	cd build && ../$(FSIM) -q --true-color --max-cycles 300000000 --log gb.log gb_image.bin </dev/null
build/gb.trace: build/gb.log build/trace2mock
	cd build && ./trace2mock gb.log gb_image.bin --tick0 $(TICK0) --ticks $(TICKS) --out gb
trace: build/gb.trace
build/font8x14.mem: $(ROOT)/sim/assets/font8x14.mem
	@mkdir -p build; grep -v '^//' $< > $@
build/mock.vvp: mock_tb.v t1f_stub.v $(RTL)
	@mkdir -p build
	$(OSS)/iverilog -g2005 -DSIM -DT1F -o $@ mock_tb.v t1f_stub.v $(RTL)
run: build/mock.vvp build/gb.trace build/font8x14.mem
	cd build && $(OSS)/vvp -n mock.vvp +trace=gb.trace +image=gb.hex +frames=$(FRAMES) +idle=$(IDLE)
clean:
	rm -rf build
.PHONY: all trace run clean

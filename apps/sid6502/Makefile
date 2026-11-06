# sid6502: 6502 interpreter and JIT for fpga_cpu.
#   make host    - host interpreter test (needs a C compiler)
#   make test    - build and run the test runner in fsim (interpreter + JIT)
ROOT   := $(abspath ../..)
FCC    := $(ROOT)/llvm/fcc
FSIM   := $(ROOT)/sim/build/fsim
PY     := $(ROOT)/.venv/bin/python
BUILD  := build
SRC    := src/interp.c src/jit.c src/jit_rt.s
SHADOW := $(BUILD)/shadow_interp.c
# large arrays: the test build lives in expansion RAM
FCCFLAGS := --base 0x100000 --stack 0x1F0000 -O2
CORE   ?= c0

all: test

tests/disk/TESTS1.BIN: tests/gen_tests.py tools/ref6502.py tools/m6502.py
	python3 tests/gen_tests.py tests/disk

src/optable.h: tools/gen_optable.py tools/m6502.py
	python3 tools/gen_optable.py

host: src/optable.h tests/disk/TESTS1.BIN
	mkdir -p $(BUILD)
	cc -O2 -Isrc -o $(BUILD)/testhost tests/testrun.c src/interp.c src/stub_host.c
	$(BUILD)/testhost tests/disk/

$(SHADOW): src/interp.c
	mkdir -p $(BUILD)
	echo '#define SHADOW 1' > $@ && echo '#include "../src/interp.c"' >> $@

$(BUILD)/testrun.bin: tests/testrun.c $(SRC) $(SHADOW) src/sid6502.h src/optable.h src/shadow.h
	mkdir -p $(BUILD)
	$(FCC) $(FCCFLAGS) -DWITH_JIT -Isrc tests/testrun.c $(SRC) $(SHADOW) -o $@

test: $(BUILD)/testrun.bin tests/disk/TESTS1.BIN
	$(FSIM) --core $(CORE) --max-cycles 40000000000 --xram 4 --load 0x100000 --disk tests/disk $(BUILD)/testrun.bin

.PHONY: all host test

$(BUILD)/bench.bin: tests/bench.c $(SRC) src/sid6502.h src/optable.h
	mkdir -p $(BUILD)
	$(FCC) $(FCCFLAGS) -Isrc tests/bench.c $(SRC) -o $@

bench: $(BUILD)/bench.bin tests/disk/MUSIC2K.BIN
	@for c in c0 c1; do echo "== core $$c"; $(FSIM) -q --core $$c --xram 4 --load 0x100000 --disk tests/disk $(BUILD)/bench.bin; done

tests/disk/MUSIC2K.BIN: tests/gen_tests.py tools/mkmusic.py
	python3 tools/mkmusic.py tests/disk

.PHONY: bench

# stress: a tiny translation cache so it is flushed all the time
$(BUILD)/testrun_small.bin: tests/testrun.c $(SRC) $(SHADOW) src/sid6502.h src/optable.h src/shadow.h
	mkdir -p $(BUILD)
	$(FCC) $(FCCFLAGS) -DWITH_JIT -DCODE_WORDS=3000 -DMAX_EV=150 -Isrc tests/testrun.c $(SRC) $(SHADOW) -o $@

test-small: $(BUILD)/testrun_small.bin
	$(FSIM) -q --core $(CORE) --max-cycles 100000000000 --xram 4 --load 0x100000 --disk tests/disk $(BUILD)/testrun_small.bin

.PHONY: test-small

# BASIC interpreter
#   make          host build: build/basic-host
#   make test     run tests/*.bas through the host build, diff against tests/*.out
#   make check    extra warning build (-Wconversion) to catch 16/32-bit mistakes
#   make bless    rewrite tests/*.out from the current output (review the diff!)
#   make target   fpga_cpu build with llvm/fcc: build/basic.A.bin, build/basic.B.bin
#   make test-target  run the tests in the simulator, both encoding variants
#   make imgcheck     JPEG/PNG decoders against reference results (ASan build)

CC      ?= cc
CFLAGS  ?= -O2 -g -std=c99 -Wall -Wextra -fno-strict-aliasing
BUILD   := build
COMMON  := src/core.c src/eval.c src/stmt.c src/num.c src/con.c src/gfx.c src/sprite.c src/image.c
HDRS    := src/basic.h src/hal.h
TESTS   := $(sort $(wildcard tests/*.bas))
SRCS    := $(COMMON) src/hal_fcpu.c
FCC     := ../llvm/fcc
# the C runtime that fcc links in: rebuild the target binaries when it changes
RUNTIME := $(FCC) $(wildcard ../llvm/runtime/asm/*.s ../llvm/runtime/libc/*.c ../llvm/runtime/libm/*.c ../llvm/runtime/include/*.h)
FSIM    := ../sim/build/fsim
VARIANTS := A B

all: $(BUILD)/basic-host

$(BUILD)/basic-host: $(COMMON) src/hal_host.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(COMMON) src/hal_host.c -lm

check: $(COMMON) src/hal_host.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Wconversion -Wsign-conversion -Wshadow -o $(BUILD)/basic-check $(COMMON) src/hal_host.c -lm

# Each test is fed to the prompt on stdin, with files in a scratch directory
# (tests/<name>.disk/ holds files for just that test).
test: $(BUILD)/basic-host
	@fail=0; for t in $(TESTS); do \
	  n=$$(basename $$t .bas); d=$(BUILD)/disk-$$n; rm -rf $$d; mkdir -p $$d; \
	  if [ -d tests/disk ]; then cp tests/disk/* $$d/ 2>/dev/null; fi; \
	  if [ -d tests/$$n.disk ]; then cp tests/$$n.disk/* $$d/; fi; \
	  BASIC_DISK=$$d BASIC_FASTFRAMES=1 $(BUILD)/basic-host -q < $$t > $(BUILD)/$$n.out 2>&1; \
	  if diff -u tests/$$n.out $(BUILD)/$$n.out > $(BUILD)/$$n.diff; then echo "$$n ok"; \
	  else echo "$$n FAILED (see $(BUILD)/$$n.diff)"; fail=1; fi; \
	done; exit $$fail

bless: $(BUILD)/basic-host
	@for t in $(TESTS); do \
	  n=$$(basename $$t .bas); d=$(BUILD)/disk-$$n; rm -rf $$d; mkdir -p $$d; \
	  if [ -d tests/disk ]; then cp tests/disk/* $$d/ 2>/dev/null; fi; \
	  if [ -d tests/$$n.disk ]; then cp tests/$$n.disk/* $$d/; fi; \
	  BASIC_DISK=$$d BASIC_FASTFRAMES=1 $(BUILD)/basic-host -q < $$t > tests/$$n.out 2>&1; \
	done

# fpga_cpu builds: basic.<v>.bin boots to the banner; basic-test.<v>.bin is
# quiet like the host's -q and has the host's 256 KB arena, for the tests.
target: $(VARIANTS:%=$(BUILD)/basic.%.bin)

$(BUILD)/basic.%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $* -O2 $(SRCS) -o $@ --stats

$(BUILD)/basic-test.%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $* -O2 -DBASIC_QUIET=1 -DBASIC_ARENA_MAX=262144UL $(SRCS) -o $@

# The same with the runtime that uses ISA extension X (benchmarks/run.py):
# basic-test-ext.<v>.bin uses all of it, basic-test-x-<part>.<v>.bin one part
# (EXT=<part>, as for fcc --ext=<part>).
$(BUILD)/basic-test-ext.%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $* --ext -O2 -DBASIC_QUIET=1 -DBASIC_ARENA_MAX=262144UL $(SRCS) -o $@

$(BUILD)/basic-test-x-%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $(lastword $(subst ., ,$*)) --ext=$(EXT) -O2 -DBASIC_QUIET=1 -DBASIC_ARENA_MAX=262144UL $(SRCS) -o $@

test-target: $(VARIANTS:%=$(BUILD)/basic-test.%.bin)
	@fail=0; for v in $(VARIANTS); do for t in $(TESTS); do \
	  n=$$(basename $$t .bas); d=$(BUILD)/tdisk-$$n.$$v; rm -rf $$d; mkdir -p $$d; \
	  if [ -d tests/disk ]; then cp tests/disk/* $$d/ 2>/dev/null; fi; \
	  if [ -d tests/$$n.disk ]; then cp tests/$$n.disk/* $$d/; fi; \
	  $(FSIM) -q -v $$v --disk $$d --max-cycles 20000000000 $(BUILD)/basic-test.$$v.bin < $$t > $(BUILD)/$$n.$$v.out 2>&1; \
	  if diff -u tests/$$n.out $(BUILD)/$$n.$$v.out > $(BUILD)/$$n.$$v.diff; then echo "$$n [$$v] ok"; \
	  else echo "$$n [$$v] FAILED (see $(BUILD)/$$n.$$v.diff)"; fail=1; fi; \
	done; done; exit $$fail

# Image decoders against reference results (needs Pillow in ../.venv).
$(BUILD)/imgdump: src/image.c tools/imgdump.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) -O1 -g -std=c99 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	  -o $@ src/image.c tools/imgdump.c

imgcheck: $(BUILD)/imgdump
	../.venv/bin/python tools/imgcheck.py

clean:
	rm -rf $(BUILD)

.PHONY: all check test bless clean target test-target imgcheck

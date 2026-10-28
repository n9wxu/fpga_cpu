# Doom for fpga_cpu (see README.md)
#   make            build/doom.A.bin and build/doom.B.bin (load at 0x100000, run with fsim --xram 14)
#   make wad        fetch the IWAD (get_wad.sh)
# The benchmark runner (../run.py doom) builds its own copies; the web
# simulator (web/) uses these.

ROOT    := ../..
FCC     := $(ROOT)/llvm/fcc
RUNTIME := $(FCC) $(wildcard $(ROOT)/llvm/runtime/asm/*.s $(ROOT)/llvm/runtime/libc/*.c \
           $(ROOT)/llvm/runtime/libm/*.c $(ROOT)/llvm/runtime/include/*.h)
SRCS    := $(sort $(wildcard src/*.c)) port/doomgeneric_fcpu.c
HDRS    := $(wildcard src/*.h port/include/*.h port/include/*/*.h)
DEFS    := -DCMAP256 -DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=360 -DNORMALUNIX
BUILD   := build

all: $(BUILD)/doom.A.bin $(BUILD)/doom.B.bin

$(BUILD)/doom.%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $* -O2 --int32 $(DEFS) -Isrc -Iport/include --cflag=-w \
	  --base 0x100000 --stack 0xF00000 $(SRCS) -o $@

wad:
	./get_wad.sh

clean:
	rm -rf $(BUILD)

.PHONY: all wad clean

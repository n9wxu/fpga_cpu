# Doom for fpga_cpu (see README.md)
#   make            build/doom.A.bin and build/doom.B.bin (load at 0x100000, run with fsim --xram 14)
#   make DRAW=c     the same with the compiled R_DrawColumn/R_DrawSpan instead of
#                   the hand-written ones in port/r_draw_fcpu.s (the default)
#   make RES=320    the original 320x200 picture instead of the full screen
#                   (640x350, -DDOOM_HIRES; the default)
#   make wad        fetch the IWAD (get_wad.sh)
# The benchmark runner (../run.py doom) builds its own copies; the web
# simulator (web/) uses these.

ROOT    := ../..
FCC     := $(ROOT)/llvm/fcc
RUNTIME := $(FCC) $(wildcard $(ROOT)/llvm/runtime/asm/*.s $(ROOT)/llvm/runtime/libc/*.c \
           $(ROOT)/llvm/runtime/libm/*.c $(ROOT)/llvm/runtime/include/*.h)
DRAW    ?= asm
RES     ?= 640
SRCS    := $(sort $(wildcard src/*.c)) port/doomgeneric_fcpu.c
ifeq ($(DRAW),asm)
SRCS    += port/r_draw_fcpu.s
DRAWDEF := -DFCPU_DRAW_ASM
endif
HDRS    := $(wildcard src/*.h port/include/*.h port/include/*/*.h)
DEFS    := -DCMAP256 -DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=360 -DNORMALUNIX
ifeq ($(RES),640)
DEFS    += -DDOOM_HIRES
endif
BUILD   := build

all: $(BUILD)/doom.A.bin $(BUILD)/doom.B.bin

$(BUILD)/doom.%.bin: $(SRCS) $(HDRS) $(RUNTIME)
	@mkdir -p $(BUILD)
	$(FCC) -q -v $* -O2 --int32 $(DEFS) $(DRAWDEF) -Isrc -Iport/include --cflag=-w \
	  --base 0x100000 --stack 0xF00000 $(SRCS) -o $@

wad:
	./get_wad.sh

clean:
	rm -rf $(BUILD)

.PHONY: all wad clean

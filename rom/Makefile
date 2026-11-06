# The ROM: BIOS, Toolbox and Shell for the Tang Nano 20K (docs/rom.md)
#   make           build/rom.img: the flash image from 0x400000 (the ROM, and the ROM disk
#                  with BASIC and romtest)
#   make test      the ROM in fsim: boot, self-test, shell commands, a program through the slots
#   make flash     the image into the Tang's flash (cd ../tang && make flash-rom)
FCC   := ../llvm/fcc
FSIM  := ../sim/build/fsim
BUILD := build
SRC   := $(wildcard src/*.c)
HDRS  := $(wildcard src/*.h) include/slots.def include/fcrom.h
# the ROM runs in the top 512 KB of the SDRAM, seen at F80000-FFFFFF; its stack at the top
ROM_BASE  := 0xF80000
ROM_STACK := 0xFFFFF0

all: $(BUILD)/rom.img

$(BUILD)/rom.bin: $(SRC) $(HDRS)
	@mkdir -p $(BUILD)
	$(FCC) -O2 -I include --base $(ROM_BASE) --stack $(ROM_STACK) $(SRC) -o $@

$(BUILD)/romtest.bin: progs/romtest.c include/fcrom.h include/slots.def
	@mkdir -p $(BUILD)
	$(FCC) -O2 -I include $< -o $@

$(BUILD)/basic.bin: $(wildcard ../basic/src/*.c ../basic/src/*.h)
	$(MAKE) -C ../basic build/basic.A.bin
	cp ../basic/build/basic.A.bin $@

$(BUILD)/rom.img: $(BUILD)/rom.bin $(BUILD)/basic.bin $(BUILD)/romtest.bin tools/mkrom.py
	python3 tools/mkrom.py $(BUILD)/rom.bin --load $(ROM_BASE) \
	  --file BASIC=$(BUILD)/basic.bin@0x1000 --file ROMTEST=$(BUILD)/romtest.bin@0x1000 \
	  -o $@ --hex $(BUILD)/rom.hex

test: $(BUILD)/rom.img
	python3 tools/romtest.py $(FSIM) $(BUILD)/rom.img

flash: $(BUILD)/rom.img
	$(MAKE) -C ../tang flash-rom ROM_IMG=../rom/$(BUILD)/rom.img

clean:
	rm -rf $(BUILD)

.PHONY: all test flash clean

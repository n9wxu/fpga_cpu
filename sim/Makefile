# fpga_cpu simulator
#   make            generate tables from cpu/isa.yaml and build build/fsim
#                   (and build/fsim-gui when SDL2 is installed)
#   make gui        build build/fsim-gui (SDL2 + Dear ImGui)
#   make test       ISA self-test, then run every benchmark on every variant
#   make measure    full measurement sweep -> build/report.md

ROOT    := ..
VENV    := $(ROOT)/.venv
PY      := $(VENV)/bin/python
CC      ?= cc
CFLAGS  ?= -O2 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter
BUILD   := build

ISA     := $(ROOT)/cpu/isa.yaml
TOOLS   := tools/isa.py tools/gen.py
CORE    := src/cpu.c src/dev.c src/video.c src/front.c src/font8x14.c src/gen/isa_gen.c src/gen/exec_gen.c
SRCS    := src/main.c $(CORE)

HAVE_SDL := $(shell command -v sdl2-config 2>/dev/null)
all: $(BUILD)/fsim $(if $(HAVE_SDL),$(BUILD)/fsim-gui)

$(PY): $(ROOT)/requirements.txt
	python3 -m venv $(VENV)
	$(VENV)/bin/pip -q install -r $(ROOT)/requirements.txt
	touch $@

$(BUILD)/gen.stamp: $(ISA) $(TOOLS) $(PY)
	@mkdir -p $(BUILD)
	$(PY) tools/gen.py
	@touch $@

src/gen/isa_gen.c src/gen/exec_gen.c src/gen/isa_gen.h: $(BUILD)/gen.stamp

$(BUILD)/fsim: $(BUILD)/gen.stamp $(SRCS) src/cpu.h src/dev.h src/front.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# ---- GUI: the C core compiled as objects, Dear ImGui and gui.cpp as C++ ----
CXX       ?= c++
CXXFLAGS  ?= -O2 -g -std=c++17 -Wall -Wno-unused-parameter
IMGUI     := third_party/imgui
IMGUI_SRC := $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp $(IMGUI)/imgui_tables.cpp $(IMGUI)/imgui_widgets.cpp \
             $(IMGUI)/backends/imgui_impl_sdl2.cpp $(IMGUI)/backends/imgui_impl_sdlrenderer2.cpp
IMGUI_OBJ := $(patsubst $(IMGUI)/%.cpp,$(BUILD)/imgui/%.o,$(IMGUI_SRC))
CORE_OBJ  := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(CORE))
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS   = $(shell sdl2-config --libs)

gui: $(BUILD)/fsim-gui

$(BUILD)/imgui/%.o: $(IMGUI)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -w $(SDL_CFLAGS) -I$(IMGUI) -c $< -o $@

$(BUILD)/obj/%.o: src/%.c $(BUILD)/gen.stamp src/cpu.h src/dev.h src/front.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fsim-gui: src/gui.cpp $(CORE_OBJ) $(IMGUI_OBJ) src/cpu.h src/dev.h src/front.h
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -I$(IMGUI) -I$(IMGUI)/backends -Isrc -o $@ src/gui.cpp $(CORE_OBJ) $(IMGUI_OBJ) $(SDL_LIBS)

test: $(BUILD)/fsim
	$(PY) tools/isa.py --selftest
	$(PY) tools/measure.py --quick
	$(PY) tools/devtest.py

measure: $(BUILD)/fsim
	$(PY) tools/measure.py

clean:
	rm -rf $(BUILD) src/gen

.PHONY: all gui test measure clean

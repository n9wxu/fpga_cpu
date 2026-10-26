# sim

Instruction-set simulator in C.

- `src/`: core (execute, timing, memory map), devices (terminal, drawing engine, RP2350 link, sound) and front ends (SDL, headless, trace, gdb stub)
- `src/gen/`: decoder and disassembler generated from `../cpu/isa.yaml`. Not committed.
- `tools/`: `gen.py` and other generators

Planned display: a 1280×720 SDL window with integer scaling, showing the exact 3-bit dithered output. `--true-color` will preview hi-color.

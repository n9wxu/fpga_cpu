# fpga_cpu

A small home computer with an 8-bit *feel*: boot to BASIC, text over
graphics, sprites, simple sound, and files on an SD card. It is not an
emulation of any existing machine.

It runs on a custom 16-bit load/store CPU designed for an LLVM backend, so
the BASIC interpreter can be written in C. The first hardware target is a
Digilent Spartan-3 Starter Board (XC3S200-FT256) plus an RP2350 expansion
board. The whole machine is simulated on the Mac before anything is
committed to the FPGA.

## Layout

| Directory  | Contents |
|------------|----------|
| `cpu/`     | CPU and ISA design: specification, opcode map, `isa.yaml` (single source of truth) |
| `sim/`     | Instruction-set simulator in C with SDL front end, plus generators driven by `isa.yaml` |
| `verilog/` | RTL (`rtl/`) and testbenches (`tb/`) for the CPU and system |
| `fpga/`    | ISE 14.7 build scripts and board constraints (UCF) |
| `pcb/`     | Expansion board (RP2350, SD, sound, I/O) |
| `basic/`   | BASIC interpreter in C |
| `llvm/`    | LLVM backend for the CPU |
| `web/`     | The simulator compiled to WebAssembly: BASIC in a browser |
| `docs/`    | System architecture and design decisions |

## Workflow

1. Define the ISA in `cpu/isa.yaml`. Generate the assembler rules and the
   simulator decoder from it.
2. Develop and measure in the simulator (`sim/`): instruction mix, I-cache
   behavior, fusion candidates.
3. Bring up the LLVM backend (`llvm/`). Write BASIC in C (`basic/`).
4. Write RTL (`verilog/`) and run it in lockstep against the simulator
   under Verilator.
5. Build with ISE 14.7 (`fpga/`), on a Linux host, and program the board
   with openFPGALoader.

See [docs/architecture.md](docs/architecture.md) for the current design.

## License

MIT. See [LICENSE](LICENSE).

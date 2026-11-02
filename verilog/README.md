# verilog

T1, the smallest core of the ladder: the whole ISA (cpu/isa.yaml, encoding
variant A) on an 8-bit datapath. [docs/t1.md](../docs/t1.md) describes it.

- `rtl/`: synthesizable Verilog-2001 (ISE 14.7 XST has no SystemVerilog, so
  stay within Verilog-2001). `t1_core.v` is the core (parameter `EXT_X`
  keeps or drops extension X), `t1_rf.v` its register file. `t1f_core.v` is
  T1F, the same ISA tuned for FPGAs ([docs/t1f.md](../docs/t1f.md)); every
  tool below runs it with `T1_CORE=t1f` in the environment.
- `tb/`: testbenches and the tools that drive them (Icarus Verilog; Python 3;
  the simulator built with `make -C sim`, and the Python environment `make`
  creates in `.venv`).
- `syn/`: transistor count (Yosys).
- `build/`: generated (images, compiled testbenches); not in git.

## Running a program

```
sim/tools/asm.py prog.s -o prog.bin          # or: llvm/fcc -o prog.bin prog.c
verilog/tb/t1run.py --lockstep prog.bin
```

`t1run.py` runs the image on the RTL (`t1_tb.v`: 1 MB of RAM, the console,
EXIT and cycle-counter registers) and on `sim/build/fsim`, and compares the
console output and how the run ended (EXIT code, `SYS n`, illegal
instruction, address fault). `--lockstep` also compares all 16 registers and
the flags after every instruction against `fsim --trace`, stopping at the
first difference; it needs a named pipe, so macOS or Linux. Other options:
`--ext-x 0` (core without extension X; fsim without `--ext`), `--reset-pc`,
`--json FILE`. It prints `T1 cycles=N insts=N exit=N`. The testbench itself
(`vvp tb.vvp +image=HEXFILE`) also takes `+busstat` and `+marker`.

## The test suites

| Command | What it checks |
|---|---|
| `tb/t1test.py` | the directed programs in `tb/tests/*.s`, each in lockstep against fsim: extension X (and that it traps with `EXT_X=0`), self-modifying code, store data before write-back, alignment faults, the end of RAM, reserved encodings, bad reset addresses |
| `tb/t1fuzz.py -n 200` | random register and memory programs (every addressing mode and size, branches, pairs, extension X), in lockstep |
| `tb/t1dec.py` | the instruction decoder against the ISA tables, on 200 000 first/second parcel pairs (`--ext-x 0` for the small build) |
| `syn/count.py --check` | `proc; check -assert` on the RTL for both `EXT_X` settings: no latches |
| `iverilog -g2001 -Wall rtl/*.v tb/t1_tb.v` | warning free |

A test program says in its header comments what it needs: `; entries: A B C`
starts it at each address in turn; `; ext: yes` also runs it without
extension X, where it must trap.

## Measuring

- `tb/t1bench.py IMAGE...`: cycles, instructions and cycles per
  instruction of whole programs (the simulator's `sim/bench/*.s` after
  `asm.py`, or `fcc` builds), with the output checked against fsim.
- `tb/t1cpi.py`: cycles of each instruction type, measured one at a time.
- `syn/count.py`: the transistor count of the core and the register file,
  in nMOS and CMOS, with and without extension X.

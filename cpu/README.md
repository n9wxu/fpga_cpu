# cpu

CPU and ISA design.

- `isa.md`: the human-readable ISA specification (formats, opcode map, flag behavior, pipeline). Not written yet.
- `isa.yaml`: the single source of truth for every instruction: mnemonic, length, fields, operands, semantics. The simulator decoder, disassembler, customasm rules and LLVM TableGen are all generated from it. Not written yet.

The current decisions are in [../docs/architecture.md](../docs/architecture.md).

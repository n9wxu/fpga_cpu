# cpu

CPU and ISA design.

- [`isa.yaml`](isa.yaml) is the single source of truth. Every instruction, its
  semantics, and every encoding of both variants are defined here. The
  simulator tables, the assembler and the encoding docs are all generated
  from it.
- [`isa.md`](isa.md) is the architecture reference: registers, flags,
  addressing modes, conventions.
- [`gen/encoding_A.md`](gen/encoding_A.md) and
  [`gen/encoding_B.md`](gen/encoding_B.md) list every encoding and the
  opcode-space usage. They are generated, so don't edit them.

Measured results are in [../docs/measurements.md](../docs/measurements.md).

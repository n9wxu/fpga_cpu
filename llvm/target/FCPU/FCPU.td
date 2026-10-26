//===-- FCPU.td - fpga_cpu target description --------------*- tablegen -*-===//
//
// fpga_cpu: 16-bit load/store CPU, 16 GPRs, register pairs as 32-bit
// values and 24-bit pointers. The instruction set is defined in
// cpu/isa.yaml; this backend emits assembly for sim/tools/asm.py, which
// picks the encoding (16- or 32-bit, variant A or B). So nothing here
// knows about encodings: every instruction is described in its most
// general form.
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

def : ProcessorModel<"generic", NoSchedModel, []>;

include "FCPURegisterInfo.td"
include "FCPUCallingConv.td"
include "FCPUInstrInfo.td"

def FCPUInstrInfo : InstrInfo;

def FCPUAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def FCPU : Target {
  let InstructionSet = FCPUInstrInfo;
  let AssemblyWriters = [FCPUAsmWriter];
}

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

// int32: clang's 32-bit int mode (fcc --int32). Only the front end uses it;
// it is declared here so the backend accepts the feature string.
def FeatureInt32 : SubtargetFeature<"int32", "HasInt32", "true",
                                    "32-bit int (front end only)">;

include "FCPURegisterInfo.td"
include "FCPUCallingConv.td"
include "FCPUInstrInfo.td"
include "FCPUSchedule.td"

def : ProcessorModel<"generic", NoSchedModel, []>;
def : ProcessorModel<"fcpu1", FCPU1Model, []>;
def : ProcessorModel<"fcpu2", FCPU2Model, []>;

def FCPUInstrInfo : InstrInfo;

def FCPUAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def FCPU : Target {
  let InstructionSet = FCPUInstrInfo;
  let AssemblyWriters = [FCPUAsmWriter];
}

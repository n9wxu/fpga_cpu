//===-- FCPUSchedule.td - fpga_cpu scheduling models -------*- tablegen -*-===//
//
// Scheduling models, chosen with fcc --cpu (clang -mcpu). Each describes a
// core configuration that fsim can time, so the compiler can be built for
// the core the hardware will have:
//
//   generic  no model: instructions stay in source order (the original
//            compiler, kept so earlier results stay reproducible)
//   fcpu1    single issue; a load's result is ready one cycle late
//            (fsim's load-use penalty), so the scheduler fills that slot
//   fcpu2    asymmetric dual issue (fsim --dual=ports --dual-any
//            --dual-ops=alu,movi,branch,shift,ptr): a full pipe, and a simple
//            pipe for register-only ALU, shift, MOVI and pointer ops and
//            branches. Either instruction of a pair may be the simple one.
//
// Compare and branch are kept adjacent (FCPUMacroFusion) so the CMP+Bcc and
// ALU+Bcc fusion of the bp1+br configurations still applies.
//
//===----------------------------------------------------------------------===//

def WriteSimple : SchedWrite; // register-only ALU, shift, MOVI, pointer ops, LEA
def WriteMul    : SchedWrite;
def WriteLoad   : SchedWrite;
def WriteStore  : SchedWrite;
def WriteBranch : SchedWrite; // Bcc, BRA
def WriteJump   : SchedWrite; // JAL, JALR, JR, RET

multiclass FCPUInstRW {
  def : InstRW<[WriteSimple], (instregex
    "^(ADD|ADC|SUB|SBC|AND|OR|XOR|SHL|SHR|SAR)(rr|Iri)$",
    "^(MOV|MOVI|NEG|NOT|CMPrr|CMPI|CPCrr|SUB32|CMP32)$",
    "^(MOVP|ADDP|ADDPU|ADDPP|ADDPI|LEAa|LEAd|LEAx)$")>;
  def : InstRW<[WriteMul], (instregex "^MUL(H|HU)?rr$")>;
  def : InstRW<[WriteLoad], (instregex "^LD(W|BU|BS|P)(a|d|x)$")>;
  def : InstRW<[WriteStore], (instregex "^ST(W|B|P)(a|d|x)$")>;
  def : InstRW<[WriteBranch], (instrs Bcc, BRA)>;
  def : InstRW<[WriteJump], (instrs JAL, JALR, JR, RET)>;
}

def FCPU1Model : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0; // in order
  let LoadLatency = 2;
  let MispredictPenalty = 1;
  let PostRAScheduler = 1;
  let CompleteModel = 0;
}

let SchedModel = FCPU1Model in {
  def F1Pipe : ProcResource<1>;
  def : WriteRes<WriteSimple, [F1Pipe]>;
  def : WriteRes<WriteMul, [F1Pipe]>;
  def : WriteRes<WriteLoad, [F1Pipe]> { let Latency = 2; }
  def : WriteRes<WriteStore, [F1Pipe]>;
  def : WriteRes<WriteBranch, [F1Pipe]>;
  def : WriteRes<WriteJump, [F1Pipe]>;
  defm : FCPUInstRW;
}

def FCPU2Model : SchedMachineModel {
  let IssueWidth = 2;
  let MicroOpBufferSize = 0;
  let LoadLatency = 2;
  let MispredictPenalty = 1;
  let PostRAScheduler = 1;
  let CompleteModel = 0;
}

let SchedModel = FCPU2Model in {
  def F2Full   : ProcResource<1>; // every instruction
  def F2Simple : ProcResource<1>; // register-only ops and branches
  def F2Any    : ProcResGroup<[F2Full, F2Simple]>;
  def : WriteRes<WriteSimple, [F2Any]>;
  def : WriteRes<WriteMul, [F2Full]>;
  def : WriteRes<WriteLoad, [F2Full]> { let Latency = 2; }
  def : WriteRes<WriteStore, [F2Full]>;
  def : WriteRes<WriteBranch, [F2Any]>;
  def : WriteRes<WriteJump, [F2Full]>;
  defm : FCPUInstRW;
}

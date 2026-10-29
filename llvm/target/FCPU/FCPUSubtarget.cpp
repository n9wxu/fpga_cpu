//===-- FCPUSubtarget.cpp - fpga_cpu subtarget ----------------------------===//

#include "FCPUSubtarget.h"
#include "FCPU.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

// Scheduling before register allocation can stretch a loop's pointer
// increments over the loads that use the old pointer, which then costs
// copies (MOVP) in the loop; after allocation it cannot.
static cl::opt<bool> PreRASched("fcpu-prera-sched", cl::init(true), cl::Hidden,
                                cl::desc("FCPU: schedule before register allocation too"));

bool FCPUSubtarget::enableMachineScheduler() const {
  return PreRASched && getSchedModel().hasInstrSchedModel();
}

#define DEBUG_TYPE "fcpu-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "FCPUGenSubtargetInfo.inc"

FCPUSubtarget::FCPUSubtarget(const Triple &TT, const std::string &CPU,
                             const std::string &FS, const TargetMachine &TM)
    : FCPUGenSubtargetInfo(TT, CPU.empty() ? "generic" : CPU,
                           CPU.empty() ? "generic" : CPU, FS),
      InstrInfo(*this), FrameLowering(*this), TLInfo(TM, *this) {}

//===-- FCPUSubtarget.cpp - fpga_cpu subtarget ----------------------------===//

#include "FCPUSubtarget.h"
#include "FCPU.h"
using namespace llvm;

#define DEBUG_TYPE "fcpu-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "FCPUGenSubtargetInfo.inc"

FCPUSubtarget::FCPUSubtarget(const Triple &TT, const std::string &CPU,
                             const std::string &FS, const TargetMachine &TM)
    : FCPUGenSubtargetInfo(TT, CPU.empty() ? "generic" : CPU,
                           CPU.empty() ? "generic" : CPU, FS),
      InstrInfo(*this), FrameLowering(*this), TLInfo(TM, *this) {}

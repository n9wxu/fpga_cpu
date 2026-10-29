//===-- FCPUSubtarget.h - fpga_cpu subtarget ---------------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUSUBTARGET_H
#define LLVM_LIB_TARGET_FCPU_FCPUSUBTARGET_H

#include "FCPUFrameLowering.h"
#include "FCPUISelLowering.h"
#include "FCPUInstrInfo.h"
#include "FCPURegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "FCPUGenSubtargetInfo.inc"

namespace llvm {

class FCPUSubtarget : public FCPUGenSubtargetInfo {
  FCPUInstrInfo InstrInfo;
  FCPUFrameLowering FrameLowering;
  FCPUTargetLowering TLInfo;
  SelectionDAGTargetInfo TSInfo;
  bool HasInt32 = false; // front-end only (clang's 32-bit int mode)

public:
  FCPUSubtarget(const Triple &TT, const std::string &CPU,
                const std::string &FS, const TargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const FCPUInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const FCPUFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const FCPUTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const FCPURegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  // Track the two halves of a pair separately, so a value read from one
  // half does not tie up (or force a copy of) the whole pair.
  bool enableSubRegLiveness() const override { return true; }
  // Schedule before and after register allocation, mainly to keep an
  // instruction from using a load result the next cycle.
  bool enableMachineScheduler() const override;
  // After allocation, generic (FCPUModel) uses the post-RA list scheduler;
  // the core models fcpu1/fcpu2 use the post-RA machine scheduler, which
  // keeps the macro-fusion pairs (FCPUTargetMachine.cpp) together.
  bool enablePostRAScheduler() const override {
    return !getSchedModel().hasInstrSchedModel();
  }
  bool enablePostRAMachineScheduler() const override {
    return getSchedModel().hasInstrSchedModel();
  }
  AntiDepBreakMode getAntiDepBreakMode() const override { return ANTIDEP_NONE; }
};

} // namespace llvm

#endif

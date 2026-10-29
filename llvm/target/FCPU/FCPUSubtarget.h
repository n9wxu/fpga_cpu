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

  // Only the fcpu1/fcpu2 models schedule; generic keeps source order.
  bool enableMachineScheduler() const override;
  bool enablePostRAScheduler() const override {
    return getSchedModel().hasInstrSchedModel();
  }
  bool enablePostRAMachineScheduler() const override {
    return getSchedModel().hasInstrSchedModel();
  }
};

} // namespace llvm

#endif

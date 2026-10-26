//===-- FCPURegisterInfo.h - fpga_cpu registers ------------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUREGISTERINFO_H
#define LLVM_LIB_TARGET_FCPU_FCPUREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "FCPUGenRegisterInfo.inc"

namespace llvm {

class FCPURegisterInfo : public FCPUGenRegisterInfo {
public:
  FCPURegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;
  const TargetRegisterClass *
  getPointerRegClass(const MachineFunction &MF,
                     unsigned Kind = 0) const override;
  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;
  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif

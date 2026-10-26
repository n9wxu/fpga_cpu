//===-- FCPUMachineFunctionInfo.h - per-function state -----------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_FCPU_FCPUMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class FCPUMachineFunctionInfo : public MachineFunctionInfo {
  int VarArgsFrameIndex = 0;
  Register SRetReturnReg;
  Register RetAddrReg; // virtual register holding the incoming lr

public:
  FCPUMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<FCPUMachineFunctionInfo>(*this);
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }
  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register Reg) { SRetReturnReg = Reg; }
  Register getRetAddrReg() const { return RetAddrReg; }
  void setRetAddrReg(Register Reg) { RetAddrReg = Reg; }
};

} // namespace llvm

#endif

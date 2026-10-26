//===-- FCPUInstrInfo.h - fpga_cpu instruction info -------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUINSTRINFO_H
#define LLVM_LIB_TARGET_FCPU_FCPUINSTRINFO_H

#include "FCPURegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "FCPUGenInstrInfo.inc"

namespace llvm {
class FCPUSubtarget;

class FCPUInstrInfo : public FCPUGenInstrInfo {
  const FCPURegisterInfo RI;

public:
  explicit FCPUInstrInfo(FCPUSubtarget &STI);

  const FCPURegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc) const override;
  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, Register SrcReg,
                           bool isKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI,
                           Register VReg) const override;
  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI, Register DestReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI,
                            Register VReg) const override;
  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;
  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  bool expandPostRAPseudo(MachineInstr &MI) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;
  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;
  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  /// Add a signed constant to a pair, in as many ADDPI steps as it takes.
  void addPairImm(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                  const DebugLoc &DL, Register Dst, Register Src, int64_t Imm,
                  MachineInstr::MIFlag Flag = MachineInstr::NoFlags) const;
};

} // namespace llvm

#endif

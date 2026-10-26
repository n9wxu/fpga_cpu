//===-- FCPURegisterInfo.cpp - fpga_cpu registers -------------------------===//

#include "FCPURegisterInfo.h"
#include "FCPU.h"
#include "FCPUSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "FCPUGenRegisterInfo.inc"

FCPURegisterInfo::FCPURegisterInfo() : FCPUGenRegisterInfo(FCPU::P6) {}

const MCPhysReg *
FCPURegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

const uint32_t *
FCPURegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  return CSR_RegMask;
}

BitVector FCPURegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector R(getNumRegs());
  for (MCPhysReg Reg : {FCPU::R0, FCPU::R1, FCPU::P0, FCPU::R14, FCPU::R15,
                        FCPU::P7, FCPU::SR})
    R.set(Reg);
  if (MF.getSubtarget().getFrameLowering()->hasFP(MF))
    for (MCPhysReg Reg : {FCPU::R10, FCPU::R11, FCPU::P5})
      R.set(Reg);
  return R;
}

const TargetRegisterClass *
FCPURegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                     unsigned Kind) const {
  return &FCPU::PRRegClass;
}

Register FCPURegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MF.getSubtarget().getFrameLowering()->hasFP(MF) ? FCPU::P5
                                                         : FCPU::P7;
}

// Frame objects are addressed [sp + off], or [fp + off] when the function
// has a frame pointer. fp holds the incoming sp, so a fixed object's
// offset is used as is. The callee-saved spills and reloads in the
// prologue and epilogue always use sp: they run while sp is known and
// before fp is set up (or after it has been reloaded).
bool FCPURegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();

  int FI = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FI);
  Register Base;
  bool Prologue = MI.getFlag(MachineInstr::FrameSetup) ||
                  MI.getFlag(MachineInstr::FrameDestroy);
  if (TFI->hasFP(MF) && !Prologue) {
    Base = FCPU::P5;
  } else {
    Base = FCPU::P7;
    Offset += MFI.getStackSize() + SPAdj;
  }

  MachineOperand &Disp = MI.getOperand(FIOperandNum + 1);
  assert(Disp.isImm() && "frame index must be followed by a displacement");
  Offset += Disp.getImm();
  if (!isInt<16>(Offset))
    report_fatal_error("FCPU: stack frame too large (offset beyond 32 KB)");

  MI.getOperand(FIOperandNum).ChangeToRegister(Base, false);
  Disp.ChangeToImmediate(Offset);
  return false;
}

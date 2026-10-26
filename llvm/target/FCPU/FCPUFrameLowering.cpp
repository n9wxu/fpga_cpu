//===-- FCPUFrameLowering.cpp - fpga_cpu stack frames ---------------------===//
//
// The stack grows down and is 2-byte aligned. A frame is allocated in one
// step:
//
//     incoming sp -> | caller's outgoing stack args (fixed objects)
//                    | callee-saved registers, locals, spills
//              sp -> | outgoing stack args for calls made here
//
// The return address lives in a virtual register (see LowerFormalArguments)
// and is spilled by the register allocator only when the function calls.
// A frame pointer (p5 = incoming sp) is used only for variable-sized
// allocas or __builtin_frame_address.
//
//===----------------------------------------------------------------------===//

#include "FCPUFrameLowering.h"
#include "FCPU.h"
#include "FCPUInstrInfo.h"
#include "FCPUSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
using namespace llvm;

FCPUFrameLowering::FCPUFrameLowering(const FCPUSubtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(2), 0,
                          Align(2)),
      STI(STI) {}

bool FCPUFrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}

bool FCPUFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

void FCPUFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const FCPUInstrInfo &TII = *STI.getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator I = MBB.begin();
  DebugLoc DL;
  uint64_t Size = alignTo(MFI.getStackSize(), 2);
  MFI.setStackSize(Size);
  if (Size)
    TII.addPairImm(MBB, I, DL, FCPU::P7, FCPU::P7, -(int64_t)Size,
                   MachineInstr::FrameSetup);
  if (hasFP(MF)) {
    // Set fp after the callee-saved spills have saved the caller's value.
    while (I != MBB.end() && I->getFlag(MachineInstr::FrameSetup))
      ++I;
    TII.addPairImm(MBB, I, DL, FCPU::P5, FCPU::P7, Size,
                   MachineInstr::FrameSetup);
  }
}

void FCPUFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const FCPUInstrInfo &TII = *STI.getInstrInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator Term = MBB.getFirstTerminator();
  DebugLoc DL = Term != MBB.end() ? Term->getDebugLoc() : DebugLoc();
  uint64_t Size = MFI.getStackSize();
  if (hasFP(MF)) {
    // Recover sp from fp before the callee-saved reloads, which use sp.
    MachineBasicBlock::iterator I = Term;
    while (I != MBB.begin() &&
           std::prev(I)->getFlag(MachineInstr::FrameDestroy))
      --I;
    TII.addPairImm(MBB, I, DL, FCPU::P7, FCPU::P5, -(int64_t)Size,
                   MachineInstr::FrameDestroy);
  }
  if (Size)
    TII.addPairImm(MBB, Term, DL, FCPU::P7, FCPU::P7, Size,
                   MachineInstr::FrameDestroy);
}

MachineBasicBlock::iterator FCPUFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  if (!hasReservedCallFrame(MF)) {
    int64_t Amount = I->getOperand(0).getImm();
    if (Amount) {
      if (I->getOpcode() == FCPU::ADJCALLSTACKDOWN)
        Amount = -Amount;
      STI.getInstrInfo()->addPairImm(MBB, I, I->getDebugLoc(), FCPU::P7,
                                     FCPU::P7, Amount);
    }
  }
  return MBB.erase(I);
}

void FCPUFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                             BitVector &SavedRegs,
                                             RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  if (hasFP(MF))
    SavedRegs.set(FCPU::P5);
}

bool FCPUFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const FCPUInstrInfo &TII = *STI.getInstrInfo();
  for (const CalleeSavedInfo &I : CSI) {
    Register Reg = I.getReg();
    MBB.addLiveIn(Reg);
    TII.storeRegToStackSlot(MBB, MI, Reg, true, I.getFrameIdx(),
                            TRI->getMinimalPhysRegClass(Reg), TRI,
                            Register());
    std::prev(MI)->setFlag(MachineInstr::FrameSetup);
  }
  return true;
}

bool FCPUFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI,
    const TargetRegisterInfo *TRI) const {
  const FCPUInstrInfo &TII = *STI.getInstrInfo();
  for (const CalleeSavedInfo &I : CSI) {
    TII.loadRegFromStackSlot(MBB, MI, I.getReg(), I.getFrameIdx(),
                             TRI->getMinimalPhysRegClass(I.getReg()), TRI,
                             Register());
    std::prev(MI)->setFlag(MachineInstr::FrameDestroy);
  }
  return true;
}

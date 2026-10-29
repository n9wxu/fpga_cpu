//===-- FCPURenameDeadDefs.cpp - free registers for flag-only results ----===//
//
// Some instructions are kept only for the flags they set: OR r, lo, hi
// tests a 32-bit value against 0, and its register result is dead. The
// register allocator still gives that dead result a register, often the
// one the surrounding code uses as its temporary (r0 in a pixel loop),
// which ties the instruction to its neighbours: it can't move between a
// load and the load's use, where it would hide the load-use stall.
//
// After register allocation, give each such dead result a register that
// nothing else in the block touches (and that is not live in or out), so
// the post-RA scheduler is free to move the instruction. Only
// caller-saved registers are used, as the prologue is already written.
//
//===----------------------------------------------------------------------===//

#include "FCPU.h"
#include "FCPUSubtarget.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "fcpu-rename-dead-defs"

namespace {
struct FCPURenameDeadDefs : public MachineFunctionPass {
  static char ID;
  FCPURenameDeadDefs() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "FCPU rename dead defs"; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // namespace

char FCPURenameDeadDefs::ID = 0;

FunctionPass *llvm::createFCPURenameDeadDefsPass() {
  return new FCPURenameDeadDefs();
}

// Caller-saved 16-bit registers, in the order to try (r13/r12 hold lr).
static const MCPhysReg Candidates[] = {FCPU::R1, FCPU::R0, FCPU::R7, FCPU::R6,
                                       FCPU::R5, FCPU::R4, FCPU::R3, FCPU::R2};

bool FCPURenameDeadDefs::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    // Registers this block never mentions and that are not live across it.
    LiveRegUnits Used(*TRI);
    Used.addLiveOuts(MBB);
    Used.addLiveIns(MBB);
    for (MachineInstr &MI : MBB)
      Used.accumulate(MI);
    for (MachineInstr &MI : MBB) {
      if (MI.getNumOperands() == 0 || !MI.getOperand(0).isReg() ||
          !MI.getOperand(0).isDef() || !MI.getOperand(0).isDead() ||
          MI.mayLoadOrStore() || MI.isCall() || MI.hasUnmodeledSideEffects())
        continue;
      Register Old = MI.getOperand(0).getReg();
      if (!FCPU::GPRRegClass.contains(Old))
        continue;
      // only worth it if the old register is used elsewhere in the block
      bool Busy = false;
      for (MachineInstr &O : MBB)
        if (&O != &MI && O.readsRegister(Old, TRI)) {
          Busy = true;
          break;
        }
      if (!Busy)
        continue;
      for (MCPhysReg R : Candidates) {
        if (!Used.available(R) || MF.getRegInfo().isReserved(R))
          continue;
        MI.getOperand(0).setReg(R);
        Used.addReg(R); // one dead def per free register
        Changed = true;
        break;
      }
    }
  }
  return Changed;
}

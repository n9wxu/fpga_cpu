//===-- FCPUInstrInfo.cpp - fpga_cpu instruction info ---------------------===//

#include "FCPUInstrInfo.h"
#include "FCPU.h"
#include "FCPUSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/MCAsmInfo.h"
using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "FCPUGenInstrInfo.inc"

FCPUInstrInfo::FCPUInstrInfo(FCPUSubtarget &STI)
    : FCPUGenInstrInfo(FCPU::ADJCALLSTACKDOWN, FCPU::ADJCALLSTACKUP), RI() {}

void FCPUInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, MCRegister DestReg,
                                MCRegister SrcReg, bool KillSrc) const {
  unsigned Opc;
  if (FCPU::GPRRegClass.contains(DestReg, SrcReg))
    Opc = FCPU::MOV;
  else if (FCPU::PRRegClass.contains(DestReg, SrcReg))
    Opc = FCPU::MOVP;
  else
    report_fatal_error("FCPU: impossible register copy");
  BuildMI(MBB, I, DL, get(Opc), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

static MachineMemOperand *frameMMO(MachineBasicBlock &MBB, int FI,
                                   MachineMemOperand::Flags F) {
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getMachineMemOperand(MachinePointerInfo::getFixedStack(MF, FI), F,
                                 MFI.getObjectSize(FI),
                                 MFI.getObjectAlign(FI));
}

void FCPUInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FI, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  unsigned Opc;
  if (RC == &FCPU::GPRRegClass)
    Opc = FCPU::STWd;
  else if (RC == &FCPU::PRRegClass)
    Opc = FCPU::STPd;
  else
    llvm_unreachable("FCPU: cannot spill this register class");
  BuildMI(MBB, MI, DL, get(Opc))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(frameMMO(MBB, FI, MachineMemOperand::MOStore));
}

void FCPUInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FI, const TargetRegisterClass *RC, const TargetRegisterInfo *TRI,
    Register VReg) const {
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  unsigned Opc;
  if (RC == &FCPU::GPRRegClass)
    Opc = FCPU::LDWd;
  else if (RC == &FCPU::PRRegClass)
    Opc = FCPU::LDPd;
  else
    llvm_unreachable("FCPU: cannot reload this register class");
  BuildMI(MBB, MI, DL, get(Opc), DestReg)
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(frameMMO(MBB, FI, MachineMemOperand::MOLoad));
}

Register FCPUInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  unsigned Opc = MI.getOpcode();
  if ((Opc == FCPU::LDWd || Opc == FCPU::LDPd) && MI.getOperand(1).isFI() &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

Register FCPUInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                           int &FrameIndex) const {
  unsigned Opc = MI.getOpcode();
  if ((Opc == FCPU::STWd || Opc == FCPU::STPd) && MI.getOperand(1).isFI() &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

bool FCPUInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  switch (MI.getOpcode()) {
  case FCPU::SUB32: {
    Register D = MI.getOperand(0).getReg(), A = MI.getOperand(1).getReg(),
             B = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(FCPU::SUBrr), RI.getSubReg(D, FCPU::sub_lo))
        .addReg(RI.getSubReg(A, FCPU::sub_lo))
        .addReg(RI.getSubReg(B, FCPU::sub_lo));
    BuildMI(MBB, MI, DL, get(FCPU::SBCrr), RI.getSubReg(D, FCPU::sub_hi))
        .addReg(RI.getSubReg(A, FCPU::sub_hi))
        .addReg(RI.getSubReg(B, FCPU::sub_hi));
    break;
  }
  case FCPU::CMP32: {
    Register A = MI.getOperand(0).getReg(), B = MI.getOperand(1).getReg();
    BuildMI(MBB, MI, DL, get(FCPU::CMPrr))
        .addReg(RI.getSubReg(A, FCPU::sub_lo))
        .addReg(RI.getSubReg(B, FCPU::sub_lo));
    BuildMI(MBB, MI, DL, get(FCPU::CPCrr))
        .addReg(RI.getSubReg(A, FCPU::sub_hi))
        .addReg(RI.getSubReg(B, FCPU::sub_hi));
    break;
  }
  default:
    return false;
  }
  MBB.erase(MI);
  return true;
}

void FCPUInstrInfo::addPairImm(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, Register Dst, Register Src,
                               int64_t Imm, MachineInstr::MIFlag Flag) const {
  do {
    int64_t Step = std::clamp<int64_t>(Imm, -32768, 32767);
    BuildMI(MBB, I, DL, get(FCPU::ADDPI), Dst)
        .addReg(Src)
        .addImm(Step)
        .setMIFlag(Flag);
    Imm -= Step;
    Src = Dst;
  } while (Imm != 0);
}

//===----------------------------------------------------------------------===//
// Branch analysis
//===----------------------------------------------------------------------===//

bool FCPUInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;
    if (!I->isBranch() || I->getOpcode() == FCPU::JR)
      return true;

    if (I->getOpcode() == FCPU::BRA) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }
      // Anything after an unconditional branch is dead.
      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    assert(I->getOpcode() == FCPU::Bcc && "unexpected branch");
    FCPUCC::CondCode CC = FCPUCC::CondCode(I->getOperand(1).getImm());
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(CC));
      continue;
    }
    // Two conditional branches: only handle the redundant case.
    assert(Cond.size() == 1 && TBB);
    if (TBB != I->getOperand(0).getMBB())
      return true;
    if (CC != FCPUCC::CondCode(Cond[0].getImm()))
      return true;
  }
  return false;
}

unsigned FCPUInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != FCPU::BRA && I->getOpcode() != FCPU::Bcc)
      break;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  return Count;
}

unsigned FCPUInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *TBB,
                                     MachineBasicBlock *FBB,
                                     ArrayRef<MachineOperand> Cond,
                                     const DebugLoc &DL,
                                     int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert(!BytesAdded && "code size not handled");
  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(FCPU::BRA)).addMBB(TBB);
    return 1;
  }
  BuildMI(&MBB, DL, get(FCPU::Bcc)).addMBB(TBB).addImm(Cond[0].getImm());
  if (!FBB)
    return 1;
  BuildMI(&MBB, DL, get(FCPU::BRA)).addMBB(FBB);
  return 2;
}

bool FCPUInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1);
  Cond[0].setImm(FCPUCC::invert(FCPUCC::CondCode(Cond[0].getImm())));
  return false;
}

unsigned FCPUInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isMetaInstruction())
    return 0;
  if (MI.isInlineAsm()) {
    const MachineFunction *MF = MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *MF->getTarget().getMCAsmInfo());
  }
  return 4;
}

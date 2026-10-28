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
  if (FCPU::GPRRegClass.hasSubClassEq(RC))
    Opc = FCPU::STWd;
  else if (FCPU::PRRegClass.hasSubClassEq(RC))
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
  if (FCPU::GPRRegClass.hasSubClassEq(RC))
    Opc = FCPU::LDWd;
  else if (FCPU::PRRegClass.hasSubClassEq(RC))
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

// CMPI r, 0 after the instruction that computed r: that instruction set
// the flags already. Logic ops set N and Z from the result and clear V,
// exactly as CMPI r, 0 does (only C differs), so every condition except
// the unsigned ones (C) reads the same. Arithmetic and shifts set Z and N
// from the result too, but V and C differently: EQ, NE, MI, PL only.
bool FCPUInstrInfo::analyzeCompare(const MachineInstr &MI, Register &SrcReg,
                                   Register &SrcReg2, int64_t &CmpMask,
                                   int64_t &CmpValue) const {
  if (MI.getOpcode() != FCPU::CMPI || !MI.getOperand(1).isImm())
    return false;
  SrcReg = MI.getOperand(0).getReg();
  SrcReg2 = Register();
  CmpMask = ~0;
  CmpValue = MI.getOperand(1).getImm();
  return true;
}

static int flagKind(unsigned Opc) {
  switch (Opc) {
  case FCPU::ANDrr: case FCPU::ORrr: case FCPU::XORrr:
  case FCPU::ANDIri: case FCPU::ORIri: case FCPU::XORIri: case FCPU::NOT:
    return 2; // logic: N, Z as CMPI 0, V = 0
  case FCPU::ADDrr: case FCPU::SUBrr: case FCPU::ADDIri: case FCPU::NEG:
  case FCPU::SHLrr: case FCPU::SHRrr: case FCPU::SARrr:
  case FCPU::SHLIri: case FCPU::SHRIri: case FCPU::SARIri:
    return 1; // N, Z only
  }
  return 0;
}

static bool ccOK(int64_t CC, int Kind) {
  switch (CC) {
  case FCPUCC::EQ: case FCPUCC::NE: case FCPUCC::MI: case FCPUCC::PL:
    return true;
  case FCPUCC::GE: case FCPUCC::LT: case FCPUCC::GT: case FCPUCC::LE:
    return Kind == 2;
  }
  return false;
}

bool FCPUInstrInfo::optimizeCompareInstr(MachineInstr &CmpInstr,
                                         Register SrcReg, Register SrcReg2,
                                         int64_t CmpMask, int64_t CmpValue,
                                         const MachineRegisterInfo *MRI) const {
  if (CmpValue != 0 || !SrcReg.isVirtual())
    return false;
  MachineInstr *Def = MRI->getUniqueVRegDef(SrcReg);
  if (!Def || Def->getParent() != CmpInstr.getParent())
    return false;
  int Kind = flagKind(Def->getOpcode());
  if (!Kind || Def->getOperand(0).getReg() != SrcReg)
    return false;
  MachineBasicBlock *MBB = CmpInstr.getParent();
  // nothing between the two may touch the flags
  for (MachineBasicBlock::iterator I = std::next(Def->getIterator());
       &*I != &CmpInstr; ++I)
    if (I->readsRegister(FCPU::SR, nullptr) ||
        I->modifiesRegister(FCPU::SR, nullptr))
      return false;
  // every reader of these flags must test a condition that comes out the same
  for (MachineBasicBlock::iterator I = std::next(CmpInstr.getIterator()),
                                   E = MBB->end();
       I != E; ++I) {
    if (I->readsRegister(FCPU::SR, nullptr)) {
      unsigned Opc = I->getOpcode();
      int64_t CC;
      if (Opc == FCPU::Bcc)
        CC = I->getOperand(1).getImm();
      else if (Opc == FCPU::SELECT16 || Opc == FCPU::SELECT32)
        CC = I->getOperand(3).getImm();
      else
        return false;
      if (!ccOK(CC, Kind))
        return false;
    }
    if (I->modifiesRegister(FCPU::SR, nullptr))
      break;
  }
  for (MachineOperand &MO : Def->operands())
    if (MO.isReg() && MO.isDef() && MO.getReg() == FCPU::SR)
      MO.setIsDead(false);
  CmpInstr.eraseFromParent();
  return true;
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

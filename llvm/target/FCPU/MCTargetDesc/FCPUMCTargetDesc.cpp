//===-- FCPUMCTargetDesc.cpp - fpga_cpu target descriptions ---------------===//

#include "FCPUMCTargetDesc.h"
#include "FCPUInstPrinter.h"
#include "FCPUMCAsmInfo.h"
#include "TargetInfo/FCPUTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "FCPUGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "FCPUGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "FCPUGenRegisterInfo.inc"

const char *llvm::FCPUCC::name(unsigned CC) {
  static const char *const Names[] = {"EQ", "NE", "HS", "LO", "MI",
                                      "PL", "VS", "VC", "HI", "LS",
                                      "GE", "LT", "GT", "LE", "RA"};
  return CC < 15 ? Names[CC] : "??";
}

FCPUCC::CondCode llvm::FCPUCC::invert(CondCode CC) {
  // Conditions come in complementary pairs: 0/1, 2/3, ... 12/13.
  return CC == AL ? AL : CondCode(CC ^ 1);
}

static MCInstrInfo *createFCPUMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitFCPUMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createFCPUMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitFCPUMCRegisterInfo(X, FCPU::P6);
  return X;
}

static MCAsmInfo *createFCPUMCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  return new FCPUMCAsmInfo(TT);
}

static MCSubtargetInfo *createFCPUMCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  return createFCPUMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCInstPrinter *createFCPUMCInstPrinter(const Triple &T,
                                              unsigned SyntaxVariant,
                                              const MCAsmInfo &MAI,
                                              const MCInstrInfo &MII,
                                              const MCRegisterInfo &MRI) {
  return SyntaxVariant == 0 ? new FCPUInstPrinter(MAI, MII, MRI) : nullptr;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeFCPUTargetMC() {
  Target &T = getTheFCPUTarget();
  TargetRegistry::RegisterMCAsmInfo(T, createFCPUMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createFCPUMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createFCPUMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createFCPUMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createFCPUMCInstPrinter);
}

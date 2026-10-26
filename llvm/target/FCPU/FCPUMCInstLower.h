//===-- FCPUMCInstLower.h - MachineInstr to MCInst --------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUMCINSTLOWER_H
#define LLVM_LIB_TARGET_FCPU_FCPUMCINSTLOWER_H

#include "llvm/Support/Compiler.h"

namespace llvm {
class AsmPrinter;
class MCContext;
class MCInst;
class MCOperand;
class MCSymbol;
class MachineInstr;
class MachineOperand;

class LLVM_LIBRARY_VISIBILITY FCPUMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

public:
  FCPUMCInstLower(MCContext &Ctx, AsmPrinter &Printer)
      : Ctx(Ctx), Printer(Printer) {}
  void lower(const MachineInstr *MI, MCInst &OutMI) const;
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const;

private:
  MCOperand lowerSymbol(const MachineOperand &MO, MCSymbol *Sym) const;
};
} // namespace llvm

#endif

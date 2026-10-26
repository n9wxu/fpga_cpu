//===-- FCPUMCInstLower.cpp - MachineInstr to MCInst ----------------------===//

#include "FCPUMCInstLower.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

MCOperand FCPUMCInstLower::lowerSymbol(const MachineOperand &MO,
                                       MCSymbol *Sym) const {
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
  return MCOperand::createExpr(Expr);
}

bool FCPUMCInstLower::lowerOperand(const MachineOperand &MO,
                                   MCOperand &MCOp) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return false;
    MCOp = MCOperand::createReg(MO.getReg());
    return true;
  case MachineOperand::MO_Immediate:
    MCOp = MCOperand::createImm(MO.getImm());
    return true;
  case MachineOperand::MO_MachineBasicBlock:
    MCOp = MCOperand::createExpr(
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));
    return true;
  case MachineOperand::MO_GlobalAddress:
    MCOp = lowerSymbol(MO, Printer.getSymbol(MO.getGlobal()));
    return true;
  case MachineOperand::MO_ExternalSymbol:
    MCOp = lowerSymbol(MO, Printer.GetExternalSymbolSymbol(MO.getSymbolName()));
    return true;
  case MachineOperand::MO_JumpTableIndex:
    MCOp = lowerSymbol(MO, Printer.GetJTISymbol(MO.getIndex()));
    return true;
  case MachineOperand::MO_ConstantPoolIndex:
    MCOp = lowerSymbol(MO, Printer.GetCPISymbol(MO.getIndex()));
    return true;
  case MachineOperand::MO_BlockAddress:
    MCOp = lowerSymbol(MO, Printer.GetBlockAddressSymbol(MO.getBlockAddress()));
    return true;
  case MachineOperand::MO_MCSymbol:
    MCOp = lowerSymbol(MO, MO.getMCSymbol());
    return true;
  case MachineOperand::MO_RegisterMask:
    return false;
  default:
    report_fatal_error("FCPU: unknown operand type in MCInst lowering");
  }
}

void FCPUMCInstLower::lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp;
    if (lowerOperand(MO, MCOp))
      OutMI.addOperand(MCOp);
  }
}

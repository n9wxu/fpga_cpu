//===-- FCPUInstPrinter.cpp - print fpga_cpu instructions -----------------===//

#include "FCPUInstPrinter.h"
#include "FCPUMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#include "FCPUGenAsmWriter.inc"

void FCPUInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) const {
  O << getRegisterName(Reg);
}

void FCPUInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &O) {
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void FCPUInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    O << getRegisterName(Op.getReg());
  else if (Op.isImm())
    O << Op.getImm();
  else
    Op.getExpr()->print(O, &MAI);
}

void FCPUInstPrinter::printCC(const MCInst *MI, unsigned OpNo,
                              raw_ostream &O) {
  O << FCPUCC::name(MI->getOperand(OpNo).getImm());
}

void FCPUInstPrinter::printMemD(const MCInst *MI, unsigned OpNo,
                                raw_ostream &O) {
  O << '[' << getRegisterName(MI->getOperand(OpNo).getReg());
  const MCOperand &D = MI->getOperand(OpNo + 1);
  if (D.isImm()) {
    int64_t V = D.getImm();
    if (V > 0)
      O << " + " << V;
    else if (V < 0)
      O << " - " << -V;
  } else {
    O << " + ";
    D.getExpr()->print(O, &MAI);
  }
  O << ']';
}

void FCPUInstPrinter::printMemX(const MCInst *MI, unsigned OpNo,
                                raw_ostream &O) {
  int64_t XM = MI->getOperand(OpNo + 2).getImm();
  O << '[' << getRegisterName(MI->getOperand(OpNo).getReg()) << " + "
    << getRegisterName(MI->getOperand(OpNo + 1).getReg());
  if (XM & 8)
    O << ".z";
  O << '*' << (XM & 7) << ']';
}

void FCPUInstPrinter::printMemA(const MCInst *MI, unsigned OpNo,
                                raw_ostream &O) {
  O << '[';
  printOperand(MI, OpNo, O);
  O << ']';
}

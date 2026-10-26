//===-- FCPUAsmPrinter.cpp - fpga_cpu assembly output ---------------------===//

#include "FCPU.h"
#include "FCPUMCInstLower.h"
#include "FCPUTargetMachine.h"
#include "MCTargetDesc/FCPUInstPrinter.h"
#include "TargetInfo/FCPUTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class FCPUAsmPrinter : public AsmPrinter {
public:
  FCPUAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "FCPU Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override {
    FCPU_MC::verifyInstructionPredicates(MI->getOpcode(),
                                        getSubtargetInfo().getFeatureBits());
    FCPUMCInstLower Lower(OutContext, *this);
    MCInst Inst;
    Lower.lower(MI, Inst);
    EmitToStreamer(*OutStreamer, Inst);
  }

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override {
    if (ExtraCode && ExtraCode[0])
      return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
    const MachineOperand &MO = MI->getOperand(OpNo);
    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      O << FCPUInstPrinter::getRegisterName(MO.getReg());
      return false;
    case MachineOperand::MO_Immediate:
      O << MO.getImm();
      return false;
    case MachineOperand::MO_GlobalAddress:
      PrintSymbolOperand(MO, O);
      return false;
    default:
      return true;
    }
  }

  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &O) override {
    const MachineOperand &Base = MI->getOperand(OpNo);
    if (!Base.isReg())
      return true;
    O << '[' << FCPUInstPrinter::getRegisterName(Base.getReg()) << ']';
    return false;
  }
};
} // namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeFCPUAsmPrinter() {
  RegisterAsmPrinter<FCPUAsmPrinter> X(getTheFCPUTarget());
}

//===-- FCPUTargetInfo.cpp - fpga_cpu target registration -----------------===//

#include "TargetInfo/FCPUTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheFCPUTarget() {
  static Target TheFCPUTarget;
  return TheFCPUTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeFCPUTargetInfo() {
  RegisterTarget<Triple::fcpu> X(getTheFCPUTarget(), "fcpu",
                                 "fpga_cpu 16-bit load/store CPU", "FCPU");
}

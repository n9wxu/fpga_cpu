//===-- FCPU.h - top-level interface for the fpga_cpu backend ---*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPU_H
#define LLVM_LIB_TARGET_FCPU_FCPU_H

#include "MCTargetDesc/FCPUMCTargetDesc.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {
class FCPUTargetMachine;
class FunctionPass;
class PassRegistry;

FunctionPass *createFCPUISelDag(FCPUTargetMachine &TM, CodeGenOptLevel OL);
void initializeFCPUDAGToDAGISelLegacyPass(PassRegistry &);
FunctionPass *createFCPUHoistGlobalLoadsPass();
void initializeFCPUHoistGlobalLoadsPass(PassRegistry &);
FunctionPass *createFCPURenameDeadDefsPass();
} // namespace llvm

#endif

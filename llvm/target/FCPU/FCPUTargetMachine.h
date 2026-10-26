//===-- FCPUTargetMachine.h - fpga_cpu target machine -----------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_FCPUTARGETMACHINE_H
#define LLVM_LIB_TARGET_FCPU_FCPUTARGETMACHINE_H

#include "FCPUSubtarget.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

namespace llvm {

class FCPUTargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  FCPUSubtarget Subtarget;

public:
  FCPUTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                    bool JIT);
  ~FCPUTargetMachine() override;

  const FCPUSubtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;
};

} // namespace llvm

#endif

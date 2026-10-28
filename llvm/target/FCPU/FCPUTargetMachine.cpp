//===-- FCPUTargetMachine.cpp - fpga_cpu target machine -------------------===//

#include "FCPUTargetMachine.h"
#include "FCPU.h"
#include "FCPUMachineFunctionInfo.h"
#include "TargetInfo/FCPUTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Transforms/Utils.h"
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeFCPUTarget() {
  RegisterTargetMachine<FCPUTargetMachine> X(getTheFCPUTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeFCPUDAGToDAGISelLegacyPass(PR);
  initializeFCPUHoistGlobalLoadsPass(PR);
}

// Must match clang/lib/Basic/Targets/FCPU.h.
static const char *DataLayoutStr =
    "e-m:e-p:32:16-i32:16-i64:16-f32:16-f64:16-a:8-n16-S16";

FCPUTargetMachine::FCPUTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : LLVMTargetMachine(T, DataLayoutStr, TT, CPU, FS, Options,
                        RM.value_or(Reloc::Static),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
  initAsmInfo();
}

FCPUTargetMachine::~FCPUTargetMachine() = default;

MachineFunctionInfo *FCPUTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return FCPUMachineFunctionInfo::create<FCPUMachineFunctionInfo>(Allocator,
                                                                  F, STI);
}

namespace {
class FCPUPassConfig : public TargetPassConfig {
public:
  FCPUPassConfig(FCPUTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  FCPUTargetMachine &getFCPUTargetMachine() const {
    return getTM<FCPUTargetMachine>();
  }

  void addIRPasses() override {
    addPass(createAtomicExpandLegacyPass());
    if (getOptLevel() != CodeGenOptLevel::None) {
      addPass(createLoopSimplifyPass());
      addPass(createFCPUHoistGlobalLoadsPass());
    }
    TargetPassConfig::addIRPasses();
  }

  void addPreSched2() override {
    if (getOptLevel() != CodeGenOptLevel::None)
      addPass(createFCPURenameDeadDefsPass());
  }

  bool addInstSelector() override {
    addPass(createFCPUISelDag(getFCPUTargetMachine(), getOptLevel()));
    return false;
  }
};
} // namespace

TargetPassConfig *FCPUTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new FCPUPassConfig(*this, PM);
}

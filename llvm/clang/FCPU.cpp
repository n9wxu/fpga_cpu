//===--- FCPU.cpp - fpga_cpu target info for clang ------------------------===//

#include "FCPU.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

static const char *const GCCRegNames[] = {
    "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "p0", "p1", "p2",  "p3",  "p4",  "p5",  "p6",  "p7"};

ArrayRef<const char *> FCPUTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void FCPUTargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__fcpu__");
  Builder.defineMacro("__FCPU__");
  Builder.defineMacro("__ELF__");
}

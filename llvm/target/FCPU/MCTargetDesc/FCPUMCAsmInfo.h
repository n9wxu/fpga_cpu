//===-- FCPUMCAsmInfo.h - fpga_cpu assembly syntax ---------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_MCTARGETDESC_FCPUMCASMINFO_H
#define LLVM_LIB_TARGET_FCPU_MCTARGETDESC_FCPUMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class FCPUMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit FCPUMCAsmInfo(const Triple &TT);
};
} // namespace llvm

#endif

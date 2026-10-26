//===-- FCPUMCTargetDesc.h - fpga_cpu target descriptions -------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_FCPU_MCTARGETDESC_FCPUMCTARGETDESC_H
#define LLVM_LIB_TARGET_FCPU_MCTARGETDESC_FCPUMCTARGETDESC_H

#include <cstdint>

namespace llvm {
// Condition codes, numbered as in cpu/isa.yaml.
namespace FCPUCC {
enum CondCode {
  EQ = 0, NE = 1, CS = 2, CC = 3, MI = 4, PL = 5, VS = 6, VC = 7,
  HI = 8, LS = 9, GE = 10, LT = 11, GT = 12, LE = 13, AL = 14
};
const char *name(unsigned CC);
CondCode invert(CondCode CC);
} // namespace FCPUCC
} // namespace llvm

#define GET_REGINFO_ENUM
#include "FCPUGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "FCPUGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "FCPUGenSubtargetInfo.inc"

#endif

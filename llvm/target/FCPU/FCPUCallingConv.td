//===-- FCPUCallingConv.td - fpga_cpu calling convention ----*- tablegen -*-===//
//
// Arguments: 16-bit values in r2-r7, 32-bit values in p1-p3 (a pair is
// skipped if either half is taken). Variadic functions pass everything on
// the stack, so va_arg is a simple pointer walk. Results: r2, or p1 (and
// p2 for 64-bit).
//
//===----------------------------------------------------------------------===//

def CC_FCPU : CallingConv<[
  CCIfType<[i8], CCPromoteToType<i16>>,
  CCIfByVal<CCPassByVal<2, 2>>,
  CCIfNotVarArg<CCIfType<[i16], CCAssignToReg<[R2, R3, R4, R5, R6, R7]>>>,
  CCIfNotVarArg<CCIfType<[i32], CCAssignToReg<[P1, P2, P3]>>>,
  CCIfType<[i16], CCAssignToStack<2, 2>>,
  CCIfType<[i32], CCAssignToStack<4, 2>>
]>;

def RetCC_FCPU : CallingConv<[
  CCIfType<[i8], CCPromoteToType<i16>>,
  CCIfType<[i16], CCAssignToReg<[R2, R3, R4, R5]>>,
  CCIfType<[i32], CCAssignToReg<[P1, P2]>>
]>;

// Saved as whole pairs, one LDP/STP each. lr is not callee-saved: the
// return address is kept in a virtual register (see LowerFormalArguments).
def CSR : CalleeSavedRegs<(add P4, P5)>;

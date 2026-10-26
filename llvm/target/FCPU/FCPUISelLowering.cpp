//===-- FCPUISelLowering.cpp - fpga_cpu DAG lowering ----------------------===//
//
// i16 lives in one register (GPR) and i32 in a pair (PR). Pointers are
// i32. i32 add uses ADDPP/ADDPI; subtract and compare are pseudos that
// expand into SUB/SBC and CMP/CPC; logic is split into halves by patterns;
// constant shifts are split here; multiply, divide and variable shifts
// are library calls. float and double are soft-float.
//
//===----------------------------------------------------------------------===//

#include "FCPUISelLowering.h"
#include "FCPU.h"
#include "FCPUMachineFunctionInfo.h"
#include "FCPUSubtarget.h"
#include "FCPUTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

#define DEBUG_TYPE "fcpu-lower"

#include "FCPUGenCallingConv.inc"

FCPUTargetLowering::FCPUTargetLowering(const TargetMachine &TM,
                                       const FCPUSubtarget &STI)
    : TargetLowering(TM), STI(STI) {
  addRegisterClass(MVT::i16, &FCPU::GPRRegClass);
  addRegisterClass(MVT::i32, &FCPU::PRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(FCPU::P7);
  setBooleanContents(ZeroOrOneBooleanContent);
  setMinFunctionAlignment(Align(2));
  setPrefFunctionAlignment(Align(2));
  setMinStackArgumentAlignment(Align(2));
  setMaxAtomicSizeInBitsSupported(0);

  for (MVT VT : {MVT::i16, MVT::i32}) {
    setOperationAction(ISD::BR_CC, VT, Custom);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
    setOperationAction(ISD::SETCC, VT, Expand);
    setOperationAction(ISD::SELECT, VT, Expand);
    setOperationAction({ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM}, VT,
                       LibCall);
    setOperationAction({ISD::SDIVREM, ISD::UDIVREM, ISD::SMUL_LOHI,
                        ISD::UMUL_LOHI, ISD::SMULO, ISD::UMULO},
                       VT, Expand);
    setOperationAction({ISD::ROTL, ISD::ROTR, ISD::BSWAP, ISD::CTPOP,
                        ISD::CTLZ, ISD::CTTZ, ISD::CTLZ_ZERO_UNDEF,
                        ISD::CTTZ_ZERO_UNDEF, ISD::BITREVERSE},
                       VT, Expand);
    setOperationAction({ISD::SHL_PARTS, ISD::SRA_PARTS, ISD::SRL_PARTS}, VT,
                       Expand);
    setOperationAction({ISD::ADDC, ISD::ADDE, ISD::SUBC, ISD::SUBE, ISD::UADDO,
                        ISD::USUBO, ISD::SADDO, ISD::SSUBO, ISD::UADDO_CARRY,
                        ISD::USUBO_CARRY},
                       VT, Expand);
    setOperationAction(ISD::DYNAMIC_STACKALLOC, VT, Expand);
  }
  setOperationAction(ISD::MUL, MVT::i32, LibCall);
  setOperationAction({ISD::MULHS, ISD::MULHU}, MVT::i32, Expand);
  setOperationAction({ISD::SHL, ISD::SRL, ISD::SRA}, MVT::i32, Custom);

  setOperationAction({ISD::BRCOND, ISD::BR_JT}, MVT::Other, Expand);
  setOperationAction({ISD::GlobalAddress, ISD::ExternalSymbol,
                      ISD::BlockAddress, ISD::JumpTable, ISD::ConstantPool},
                     MVT::i32, Custom);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction({ISD::VAARG, ISD::VACOPY, ISD::VAEND}, MVT::Other,
                     Expand);
  setOperationAction({ISD::STACKSAVE, ISD::STACKRESTORE}, MVT::Other, Expand);
  setOperationAction({ISD::FRAMEADDR, ISD::RETURNADDR}, MVT::i32, Custom);
  setOperationAction(ISD::SIGN_EXTEND_INREG, {MVT::i1, MVT::i8}, Expand);
  // i32 from i16: keep the low half, high half = low >> 15 (arithmetic).
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Custom);

  for (MVT VT : MVT::integer_valuetypes())
    setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, VT,
                     MVT::i1, Promote);
  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, MVT::i32,
                   {MVT::i8, MVT::i16}, Expand);
  setTruncStoreAction(MVT::i32, MVT::i16, Expand);
  setTruncStoreAction(MVT::i32, MVT::i8, Expand);
}

const char *FCPUTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((FCPUISD::NodeType)Opcode) {
  case FCPUISD::FIRST_NUMBER: break;
  case FCPUISD::CALL: return "FCPUISD::CALL";
  case FCPUISD::RET_GLUE: return "FCPUISD::RET_GLUE";
  case FCPUISD::CMP: return "FCPUISD::CMP";
  case FCPUISD::BR_CC: return "FCPUISD::BR_CC";
  case FCPUISD::SELECT_CC: return "FCPUISD::SELECT_CC";
  case FCPUISD::Wrapper: return "FCPUISD::Wrapper";
  }
  return nullptr;
}

bool FCPUTargetLowering::isTruncateFree(Type *Ty1, Type *Ty2) const {
  return Ty1->isIntegerTy(32) && Ty2->isIntegerTy(16);
}

bool FCPUTargetLowering::isTruncateFree(EVT VT1, EVT VT2) const {
  return VT1 == MVT::i32 && VT2 == MVT::i16;
}

// [pN + d16] and [abs24]. The indexed mode takes a 16-bit index, which
// loop strength reduction cannot express, so it is not offered here.
bool FCPUTargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                               const AddrMode &AM, Type *Ty,
                                               unsigned AS,
                                               Instruction *I) const {
  if (AM.BaseGV)
    return !AM.HasBaseReg && AM.Scale == 0;
  if (!isInt<16>(AM.BaseOffs))
    return false;
  if (AM.Scale == 0)
    return true;
  return AM.Scale == 1 && !AM.HasBaseReg;
}

//===----------------------------------------------------------------------===//
// Arguments, calls and returns
//===----------------------------------------------------------------------===//

SDValue FCPUTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  auto *FuncInfo = MF.getInfo<FCPUMachineFunctionInfo>();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_FCPU);

  // The return address is a value like any other: copied out of lr on
  // entry and back just before RET, so the register allocator decides
  // whether it stays in a register or is spilled around calls.
  FuncInfo->setRetAddrReg(MF.addLiveIn(FCPU::P6, &FCPU::PRRegClass));

  if (IsVarArg)
    FuncInfo->setVarArgsFrameIndex(
        MFI.CreateFixedObject(2, CCInfo.getStackSize(), true));

  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    CCValAssign &VA = ArgLocs[I];
    if (VA.isRegLoc()) {
      EVT RegVT = VA.getLocVT();
      const TargetRegisterClass *RC =
          RegVT == MVT::i32 ? &FCPU::PRRegClass : &FCPU::GPRRegClass;
      Register VReg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue V = DAG.getCopyFromReg(Chain, DL, VReg, RegVT);
      if (VA.getLocInfo() == CCValAssign::SExt)
        V = DAG.getNode(ISD::AssertSext, DL, RegVT, V,
                        DAG.getValueType(VA.getValVT()));
      else if (VA.getLocInfo() == CCValAssign::ZExt)
        V = DAG.getNode(ISD::AssertZext, DL, RegVT, V,
                        DAG.getValueType(VA.getValVT()));
      if (VA.getLocInfo() != CCValAssign::Full)
        V = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), V);
      InVals.push_back(V);
      continue;
    }

    assert(VA.isMemLoc());
    ISD::ArgFlagsTy Flags = Ins[I].Flags;
    if (Flags.isByVal()) {
      int FI = MFI.CreateFixedObject(Flags.getByValSize(),
                                     VA.getLocMemOffset(), true);
      InVals.push_back(DAG.getFrameIndex(FI, MVT::i32));
      continue;
    }
    unsigned Size = VA.getLocVT().getStoreSize();
    int FI = MFI.CreateFixedObject(Size, VA.getLocMemOffset(), true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
    SDValue V = DAG.getLoad(VA.getLocVT(), DL, Chain, FIN,
                            MachinePointerInfo::getFixedStack(MF, FI));
    if (VA.getLocInfo() != CCValAssign::Full)
      V = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), V);
    InVals.push_back(V);
  }
  return Chain;
}

SDValue FCPUTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  MachineFunction &MF = DAG.getMachineFunction();
  CLI.IsTailCall = false;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_FCPU);
  unsigned NumBytes = CCInfo.getStackSize();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 6> RegsToPass;
  SmallVector<SDValue, 8> MemOps;
  SDValue SP;
  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    CCValAssign &VA = ArgLocs[I];
    SDValue Arg = OutVals[I];
    switch (VA.getLocInfo()) {
    case CCValAssign::Full: break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    default: llvm_unreachable("unknown argument extension");
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
      continue;
    }
    if (!SP.getNode())
      SP = DAG.getCopyFromReg(Chain, DL, FCPU::P7, MVT::i32);
    SDValue Addr = DAG.getNode(ISD::ADD, DL, MVT::i32, SP,
                               DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));
    ISD::ArgFlagsTy Flags = Outs[I].Flags;
    if (Flags.isByVal()) {
      SDValue Size = DAG.getConstant(Flags.getByValSize(), DL, MVT::i32);
      MemOps.push_back(DAG.getMemcpy(
          Chain, DL, Addr, Arg, Size, Flags.getNonZeroByValAlign(),
          /*isVol=*/false, /*AlwaysInline=*/true, /*CI=*/nullptr,
          std::nullopt, MachinePointerInfo(), MachinePointerInfo()));
    } else {
      MemOps.push_back(DAG.getStore(Chain, DL, Arg, Addr, MachinePointerInfo()));
    }
  }
  if (!MemOps.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOps);

  SDValue Glue;
  for (auto &R : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, R.first, R.second, Glue);
    Glue = Chain.getValue(1);
  }

  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                        G->getOffset());
  else if (auto *S = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), MVT::i32);

  SmallVector<SDValue, 8> Ops = {Chain, Callee};
  for (auto &R : RegsToPass)
    Ops.push_back(DAG.getRegister(R.first, R.second.getValueType()));
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  Ops.push_back(DAG.getRegisterMask(TRI->getCallPreservedMask(MF, CLI.CallConv)));
  if (Glue.getNode())
    Ops.push_back(Glue);

  Chain = DAG.getNode(FCPUISD::CALL, DL, DAG.getVTList(MVT::Other, MVT::Glue),
                      Ops);
  Glue = Chain.getValue(1);
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  return LowerCallResult(Chain, Glue, CLI.CallConv, CLI.IsVarArg, CLI.Ins, DL,
                         DAG, InVals);
}

SDValue FCPUTargetLowering::LowerCallResult(
    SDValue Chain, SDValue Glue, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallResult(Ins, RetCC_FCPU);
  for (CCValAssign &VA : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(), Glue)
                .getValue(1);
    Glue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }
  return Chain;
}

bool FCPUTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_FCPU);
}

SDValue
FCPUTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool IsVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &DL, SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_FCPU);

  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<FCPUMachineFunctionInfo>();
  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  SDValue RA = DAG.getCopyFromReg(Chain, DL, FuncInfo->getRetAddrReg(),
                                  MVT::i32);
  Chain = DAG.getCopyToReg(RA.getValue(1), DL, FCPU::P6, RA, Glue);
  Glue = Chain.getValue(1);
  RetOps.push_back(DAG.getRegister(FCPU::P6, MVT::i32));
  for (unsigned I = 0; I != RVLocs.size(); ++I) {
    CCValAssign &VA = RVLocs[I];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[I], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }
  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);
  return DAG.getNode(FCPUISD::RET_GLUE, DL, MVT::Other, RetOps);
}

//===----------------------------------------------------------------------===//
// Custom operations
//===----------------------------------------------------------------------===//

SDValue FCPUTargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
  case ISD::ExternalSymbol:
  case ISD::BlockAddress:
  case ISD::JumpTable:
  case ISD::ConstantPool:
    return lowerSymbol(Op, DAG);
  case ISD::BR_CC:
    return lowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:
    return lowerSELECT_CC(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return lowerShift32(Op, DAG);
  case ISD::SIGN_EXTEND_INREG: {
    SDLoc DL(Op);
    SDValue Lo = DAG.getTargetExtractSubreg(FCPU::sub_lo, DL, MVT::i16,
                                            Op.getOperand(0));
    SDValue Hi = DAG.getNode(ISD::SRA, DL, MVT::i16, Lo,
                             DAG.getConstant(15, DL, MVT::i16));
    SDValue Ops[] = {DAG.getTargetConstant(FCPU::PRRegClassID, DL, MVT::i32),
                     Lo, DAG.getTargetConstant(FCPU::sub_lo, DL, MVT::i32), Hi,
                     DAG.getTargetConstant(FCPU::sub_hi, DL, MVT::i32)};
    return SDValue(DAG.getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::i32,
                                      Ops), 0);
  }
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::FRAMEADDR:
    return lowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return lowerRETURNADDR(Op, DAG);
  default:
    llvm_unreachable("unexpected custom lowering");
  }
}

SDValue FCPUTargetLowering::lowerSymbol(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue T;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Op))
    T = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                   G->getOffset());
  else if (auto *S = dyn_cast<ExternalSymbolSDNode>(Op))
    T = DAG.getTargetExternalSymbol(S->getSymbol(), MVT::i32);
  else if (auto *B = dyn_cast<BlockAddressSDNode>(Op))
    T = DAG.getTargetBlockAddress(B->getBlockAddress(), MVT::i32,
                                  B->getOffset());
  else if (auto *J = dyn_cast<JumpTableSDNode>(Op))
    T = DAG.getTargetJumpTable(J->getIndex(), MVT::i32);
  else if (auto *C = dyn_cast<ConstantPoolSDNode>(Op))
    T = DAG.getTargetConstantPool(C->getConstVal(), MVT::i32, C->getAlign(),
                                  C->getOffset());
  else
    llvm_unreachable("not a symbol");
  return DAG.getNode(FCPUISD::Wrapper, DL, MVT::i32, T);
}

static FCPUCC::CondCode mapCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ: return FCPUCC::EQ;
  case ISD::SETNE: return FCPUCC::NE;
  case ISD::SETLT: return FCPUCC::LT;
  case ISD::SETLE: return FCPUCC::LE;
  case ISD::SETGT: return FCPUCC::GT;
  case ISD::SETGE: return FCPUCC::GE;
  case ISD::SETULT: return FCPUCC::CC;
  case ISD::SETULE: return FCPUCC::LS;
  case ISD::SETUGT: return FCPUCC::HI;
  case ISD::SETUGE: return FCPUCC::CS;
  default: llvm_unreachable("unsupported integer condition");
  }
}

// Compare LHS with RHS; returns the flags as glue and sets TargetCC.
static SDValue emitCmp(SDValue LHS, SDValue RHS, ISD::CondCode CC,
                       SDValue &TargetCC, const SDLoc &DL, SelectionDAG &DAG) {
  // A constant on the left: swap, so the immediate compare form applies.
  if (isa<ConstantSDNode>(LHS) && !isa<ConstantSDNode>(RHS)) {
    std::swap(LHS, RHS);
    CC = ISD::getSetCCSwappedOperands(CC);
  }
  TargetCC = DAG.getTargetConstant(mapCC(CC), DL, MVT::i16);
  return DAG.getNode(FCPUISD::CMP, DL, MVT::Glue, LHS, RHS);
}

SDValue FCPUTargetLowering::lowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue TargetCC;
  SDValue Flags = emitCmp(Op.getOperand(2), Op.getOperand(3), CC, TargetCC,
                          DL, DAG);
  return DAG.getNode(FCPUISD::BR_CC, DL, MVT::Other, Chain, Op.getOperand(4),
                     TargetCC, Flags);
}

SDValue FCPUTargetLowering::lowerSELECT_CC(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDValue TargetCC;
  SDValue Flags = emitCmp(Op.getOperand(0), Op.getOperand(1), CC, TargetCC,
                          DL, DAG);
  return DAG.getNode(FCPUISD::SELECT_CC, DL, Op.getValueType(),
                     Op.getOperand(2), Op.getOperand(3), TargetCC, Flags);
}

static SDValue half(SelectionDAG &DAG, const SDLoc &DL, SDValue V,
                    unsigned Idx) {
  return DAG.getTargetExtractSubreg(Idx, DL, MVT::i16, V);
}

static SDValue join(SelectionDAG &DAG, const SDLoc &DL, SDValue Lo,
                    SDValue Hi) {
  SDValue Ops[] = {DAG.getTargetConstant(FCPU::PRRegClassID, DL, MVT::i32), Lo,
                   DAG.getTargetConstant(FCPU::sub_lo, DL, MVT::i32), Hi,
                   DAG.getTargetConstant(FCPU::sub_hi, DL, MVT::i32)};
  return SDValue(
      DAG.getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::i32, Ops), 0);
}

// 32-bit shifts by a constant become 16-bit shifts of the halves; others
// call __ashlsi3 / __lshrsi3 / __ashrsi3.
SDValue FCPUTargetLowering::lowerShift32(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  unsigned Opc = Op.getOpcode();
  SDValue X = Op.getOperand(0);
  auto *C = dyn_cast<ConstantSDNode>(Op.getOperand(1));
  if (!C) {
    RTLIB::Libcall LC = Opc == ISD::SHL   ? RTLIB::SHL_I32
                        : Opc == ISD::SRL ? RTLIB::SRL_I32
                                          : RTLIB::SRA_I32;
    MakeLibCallOptions Opts;
    SDValue Ops[] = {X, Op.getOperand(1)};
    return makeLibCall(DAG, LC, MVT::i32, Ops, Opts, DL).first;
  }

  unsigned N = C->getZExtValue() & 31;
  if (N == 0)
    return X;
  // Keep a scaled 16-bit index intact for the [pN + rX*s] mode.
  if (Opc == ISD::SHL && (N == 1 || N == 2) &&
      (X.getOpcode() == ISD::SIGN_EXTEND ||
       X.getOpcode() == ISD::ZERO_EXTEND) &&
      X.getOperand(0).getValueType() == MVT::i16)
    return Op;
  auto Sh = [&](unsigned O, SDValue V, unsigned K) {
    return DAG.getNode(O, DL, MVT::i16, V, DAG.getConstant(K, DL, MVT::i16));
  };
  SDValue Lo = half(DAG, DL, X, FCPU::sub_lo);
  SDValue Hi = half(DAG, DL, X, FCPU::sub_hi);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
  SDValue RLo, RHi;
  if (Opc == ISD::SHL) {
    if (N >= 16) {
      RLo = Zero;
      RHi = N == 16 ? Lo : Sh(ISD::SHL, Lo, N - 16);
    } else {
      RLo = Sh(ISD::SHL, Lo, N);
      RHi = DAG.getNode(ISD::OR, DL, MVT::i16, Sh(ISD::SHL, Hi, N),
                        Sh(ISD::SRL, Lo, 16 - N));
    }
  } else {
    unsigned HOp = Opc == ISD::SRA ? ISD::SRA : ISD::SRL;
    if (N >= 16) {
      RLo = N == 16 ? Hi : Sh(HOp, Hi, N - 16);
      RHi = Opc == ISD::SRA ? Sh(ISD::SRA, Hi, 15) : Zero;
    } else {
      RLo = DAG.getNode(ISD::OR, DL, MVT::i16, Sh(ISD::SRL, Lo, N),
                        Sh(ISD::SHL, Hi, 16 - N));
      RHi = Sh(HOp, Hi, N);
    }
  }
  return join(DAG, DL, RLo, RHi);
}

SDValue FCPUTargetLowering::lowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<FCPUMachineFunctionInfo>();
  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), MVT::i32);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue FCPUTargetLowering::lowerFRAMEADDR(SDValue Op,
                                           SelectionDAG &DAG) const {
  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setFrameAddressIsTaken(true);
  if (Op.getConstantOperandVal(0) != 0)
    report_fatal_error("FCPU: __builtin_frame_address only supports depth 0");
  return DAG.getCopyFromReg(DAG.getEntryNode(), SDLoc(Op), FCPU::P5, MVT::i32);
}

SDValue FCPUTargetLowering::lowerRETURNADDR(SDValue Op,
                                            SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MF.getFrameInfo().setReturnAddressIsTaken(true);
  if (Op.getConstantOperandVal(0) != 0)
    report_fatal_error("FCPU: __builtin_return_address only supports depth 0");
  Register Reg = MF.addLiveIn(FCPU::P6, &FCPU::PRRegClass);
  return DAG.getCopyFromReg(DAG.getEntryNode(), SDLoc(Op), Reg, MVT::i32);
}

//===----------------------------------------------------------------------===//
// SELECT16 / SELECT32: a branch diamond
//===----------------------------------------------------------------------===//

MachineBasicBlock *
FCPUTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                MachineBasicBlock *BB) const {
  assert((MI.getOpcode() == FCPU::SELECT16 ||
          MI.getOpcode() == FCPU::SELECT32) &&
         "unexpected custom-inserted instruction");
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  const BasicBlock *LLVMBB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();
  MachineFunction *F = BB->getParent();

  //   ThisMBB: Bcc SinkMBB          (true value already in $t)
  //   FalseMBB: (fall through)
  //   SinkMBB: phi [$t, ThisMBB], [$f, FalseMBB]
  MachineBasicBlock *ThisMBB = BB;
  MachineBasicBlock *FalseMBB = F->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *SinkMBB = F->CreateMachineBasicBlock(LLVMBB);
  F->insert(It, FalseMBB);
  F->insert(It, SinkMBB);
  SinkMBB->splice(SinkMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(BB);
  BB->addSuccessor(FalseMBB);
  BB->addSuccessor(SinkMBB);
  BuildMI(BB, DL, TII.get(FCPU::Bcc))
      .addMBB(SinkMBB)
      .addImm(MI.getOperand(3).getImm());
  FalseMBB->addSuccessor(SinkMBB);

  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(FCPU::PHI),
          MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg())
      .addMBB(ThisMBB)
      .addReg(MI.getOperand(2).getReg())
      .addMBB(FalseMBB);

  MI.eraseFromParent();
  return SinkMBB;
}

//===----------------------------------------------------------------------===//
// Inline assembly
//===----------------------------------------------------------------------===//

TargetLowering::ConstraintType
FCPUTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1 && Constraint[0] == 'p')
    return C_RegisterClass;
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
FCPUTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                 StringRef Constraint,
                                                 MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      if (VT == MVT::i32)
        return {0U, &FCPU::PRRegClass};
      return {0U, &FCPU::GPRRegClass};
    case 'p':
      return {0U, &FCPU::PRRegClass};
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

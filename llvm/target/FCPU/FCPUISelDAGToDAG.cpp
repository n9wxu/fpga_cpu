//===-- FCPUISelDAGToDAG.cpp - fpga_cpu instruction selection -------------===//
//
// Addressing modes:
//   d: [pN + d16]            base pair (or frame index) + signed 16 bits
//   x: [pN + rX(.z)*s]       base pair + sign/zero-extended 16-bit index
//   a: [abs24]               symbol + offset, or a constant address
//
//===----------------------------------------------------------------------===//

#include "FCPU.h"
#include "FCPUISelLowering.h"
#include "FCPUTargetMachine.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/InlineAsm.h"
using namespace llvm;

#define DEBUG_TYPE "fcpu-isel"
#define PASS_NAME "FCPU DAG->DAG Pattern Instruction Selection"

namespace {
class FCPUDAGToDAGISel : public SelectionDAGISel {
public:
  FCPUDAGToDAGISel() = delete;
  FCPUDAGToDAGISel(FCPUTargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISel(TM, OL) {}

  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override {
    OutOps.push_back(Op);
    return false;
  }

#include "FCPUGenDAGISel.inc"

private:
  void Select(SDNode *N) override;
  bool SelectAddrD(SDValue N, SDValue &Base, SDValue &Disp);
  bool SelectAddrA(SDValue N, SDValue &Abs);
  bool SelectAddrX(SDValue N, SDValue &Base, SDValue &Idx, SDValue &XM);
  bool SelectAddrXScaled(SDValue N, SDValue &Base, SDValue &Idx,
                         SDValue &XM);
  bool matchIndex(SDValue N, SDValue &Base, SDValue &Idx, SDValue &XM,
                  bool NeedScale);
  SDValue imm16(int64_t V, const SDLoc &DL) {
    return CurDAG->getTargetConstant(V, DL, MVT::i16);
  }
};

class FCPUDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  FCPUDAGToDAGISelLegacy(FCPUTargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISelLegacy(ID,
                               std::make_unique<FCPUDAGToDAGISel>(TM, OL)) {}
};
} // namespace

char FCPUDAGToDAGISelLegacy::ID;

INITIALIZE_PASS(FCPUDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createFCPUISelDag(FCPUTargetMachine &TM,
                                      CodeGenOptLevel OL) {
  return new FCPUDAGToDAGISelLegacy(TM, OL);
}

static bool isWrapperBased(SDValue N, SelectionDAG &DAG) {
  if (N.getOpcode() == FCPUISD::Wrapper)
    return true;
  return DAG.isBaseWithConstantOffset(N) &&
         N.getOperand(0).getOpcode() == FCPUISD::Wrapper;
}

bool FCPUDAGToDAGISel::SelectAddrD(SDValue N, SDValue &Base, SDValue &Disp) {
  SDLoc DL(N);
  if (isWrapperBased(N, *CurDAG) || isa<ConstantSDNode>(N))
    return false;
  int64_t Off = 0;
  if (CurDAG->isBaseWithConstantOffset(N)) {
    int64_t C = cast<ConstantSDNode>(N.getOperand(1))->getSExtValue();
    if (isInt<16>(C)) {
      Off = C;
      N = N.getOperand(0);
    }
  }
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(N))
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
  else
    Base = N;
  Disp = imm16(Off, DL);
  return true;
}

bool FCPUDAGToDAGISel::SelectAddrA(SDValue N, SDValue &Abs) {
  SDLoc DL(N);
  if (auto *C = dyn_cast<ConstantSDNode>(N)) {
    if (!isUInt<24>(C->getZExtValue()))
      return false;
    Abs = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i32);
    return true;
  }
  int64_t Off = 0;
  if (CurDAG->isBaseWithConstantOffset(N) &&
      N.getOperand(0).getOpcode() == FCPUISD::Wrapper) {
    Off = cast<ConstantSDNode>(N.getOperand(1))->getSExtValue();
    N = N.getOperand(0);
  }
  if (N.getOpcode() != FCPUISD::Wrapper)
    return false;
  SDValue T = N.getOperand(0);
  if (auto *G = dyn_cast<GlobalAddressSDNode>(T)) {
    Abs = CurDAG->getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                         G->getOffset() + Off);
    return true;
  }
  if (Off)
    return false;
  if (isa<ExternalSymbolSDNode>(T) || isa<BlockAddressSDNode>(T) ||
      isa<JumpTableSDNode>(T) || isa<ConstantPoolSDNode>(T)) {
    Abs = T;
    return true;
  }
  return false;
}

// (add Base, (shl (sext|zext i16:X), k)), k = 0, 1 or 2, either order.
bool FCPUDAGToDAGISel::matchIndex(SDValue N, SDValue &Base, SDValue &Idx,
                                  SDValue &XM, bool NeedScale) {
  if (N.getOpcode() != ISD::ADD)
    return false;
  for (unsigned Side = 0; Side < 2; ++Side) {
    SDValue B = N.getOperand(Side), I = N.getOperand(1 - Side);
    unsigned Scale = 1;
    if (I.getOpcode() == ISD::SHL && isa<ConstantSDNode>(I.getOperand(1))) {
      uint64_t K = I.getConstantOperandVal(1);
      if (K != 1 && K != 2)
        continue;
      Scale = 1u << K;
      I = I.getOperand(0);
    }
    if (NeedScale && Scale == 1)
      continue;
    unsigned Ext;
    if (I.getOpcode() == ISD::SIGN_EXTEND)
      Ext = 0;
    else if (I.getOpcode() == ISD::ZERO_EXTEND)
      Ext = 8;
    else
      continue;
    if (I.getOperand(0).getValueType() != MVT::i16)
      continue;
    Base = B;
    Idx = I.getOperand(0);
    XM = imm16(Scale | Ext, SDLoc(N));
    return true;
  }
  return false;
}

bool FCPUDAGToDAGISel::SelectAddrX(SDValue N, SDValue &Base, SDValue &Idx,
                                   SDValue &XM) {
  return matchIndex(N, Base, Idx, XM, false);
}

bool FCPUDAGToDAGISel::SelectAddrXScaled(SDValue N, SDValue &Base,
                                         SDValue &Idx, SDValue &XM) {
  return matchIndex(N, Base, Idx, XM, true);
}

void FCPUDAGToDAGISel::Select(SDNode *N) {
  SDLoc DL(N);
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  // Frame addresses: LEA pN, [sp + off] (rewritten once the frame is laid
  // out).
  int64_t Off = 0;
  SDNode *FIN = nullptr;
  if (N->getOpcode() == ISD::FrameIndex) {
    FIN = N;
  } else if (N->getOpcode() == ISD::ADD &&
             isa<FrameIndexSDNode>(N->getOperand(0)) &&
             isa<ConstantSDNode>(N->getOperand(1))) {
    int64_t C = cast<ConstantSDNode>(N->getOperand(1))->getSExtValue();
    if (isInt<16>(C)) {
      FIN = N->getOperand(0).getNode();
      Off = C;
    }
  }
  if (FIN) {
    int FI = cast<FrameIndexSDNode>(FIN)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i32);
    CurDAG->SelectNodeTo(N, FCPU::LEAd, MVT::i32, TFI, imm16(Off, DL));
    return;
  }

  SelectCode(N);
}

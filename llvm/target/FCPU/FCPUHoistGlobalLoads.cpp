//===-- FCPUHoistGlobalLoads.cpp - hoist global loads past stores ---------===//
//
// Old C keeps loop state in globals: Doom's R_DrawColumn reads dc_source
// and dc_colormap, two global pointers, for every pixel it stores. C lets
// a byte store change any object, so LICM can't hoist those loads, and on
// a machine with only six free register pairs every reload costs a load
// and usually a load-use stall. LLVM's LoopVersioningLICM would version
// the loop, but it gives up when the loop loads pointers and stores bytes.
//
// This pass handles the simple, common shape: an innermost loop whose
// stores all walk memory with a constant step (pointer = {start,+,step}),
// that calls nothing that writes memory, and that loads from global
// variables. Before the loop it checks, once, that the bytes the stores
// will touch don't overlap those globals; if so a copy of the loop runs
// with the global loads hoisted into its preheader, otherwise the original
// loop runs unchanged.
//
//===----------------------------------------------------------------------===//

#include "FCPU.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

using namespace llvm;

#define DEBUG_TYPE "fcpu-hoist-global-loads"

static cl::opt<bool> Disable("fcpu-no-hoist-global-loads", cl::Hidden,
                             cl::desc("Turn off FCPUHoistGlobalLoads"));

namespace {

struct FCPUHoistGlobalLoads : public FunctionPass {
  static char ID;
  FCPUHoistGlobalLoads() : FunctionPass(ID) {
    initializeFCPUHoistGlobalLoadsPass(*PassRegistry::getPassRegistry());
  }
  StringRef getPassName() const override { return "FCPU hoist global loads"; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }
  bool runOnFunction(Function &F) override;

private:
  bool processLoop(Loop *L, DominatorTree &DT, LoopInfo &LI,
                   ScalarEvolution &SE);
};

} // namespace

char FCPUHoistGlobalLoads::ID = 0;

INITIALIZE_PASS_BEGIN(FCPUHoistGlobalLoads, DEBUG_TYPE,
                      "FCPU hoist global loads", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_END(FCPUHoistGlobalLoads, DEBUG_TYPE,
                    "FCPU hoist global loads", false, false)

FunctionPass *llvm::createFCPUHoistGlobalLoadsPass() {
  return new FCPUHoistGlobalLoads();
}

bool FCPUHoistGlobalLoads::runOnFunction(Function &F) {
  if (Disable || skipFunction(F))
    return false;
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  SmallVector<Loop *, 8> Inner;
  for (Loop *L : LI.getLoopsInPreorder())
    if (L->isInnermost())
      Inner.push_back(L);
  bool Changed = false;
  for (Loop *L : Inner)
    Changed |= processLoop(L, DT, LI, SE);
  return Changed;
}

bool FCPUHoistGlobalLoads::processLoop(Loop *L, DominatorTree &DT,
                                       LoopInfo &LI, ScalarEvolution &SE) {
  if (!L->isLoopSimplifyForm() || !L->getExitBlock() ||
      L->getExitingBlock() != L->getLoopLatch())
    return false;
  const SCEV *BTC = SE.getBackedgeTakenCount(L);
  if (isa<SCEVCouldNotCompute>(BTC))
    return false;

  SmallVector<LoadInst *, 4> GLoads;
  SmallVector<std::pair<const SCEVAddRecExpr *, uint64_t>, 4> Stores;
  const DataLayout &DLay = L->getHeader()->getDataLayout();
  for (BasicBlock *BB : L->blocks())
    for (Instruction &I : *BB) {
      if (auto *Ld = dyn_cast<LoadInst>(&I)) {
        if (!Ld->isSimple())
          return false;
        if (isa<GlobalVariable>(Ld->getPointerOperand()))
          GLoads.push_back(Ld);
      } else if (auto *St = dyn_cast<StoreInst>(&I)) {
        if (!St->isSimple())
          return false;
        auto *AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(St->getPointerOperand()));
        if (!AR || AR->getLoop() != L || !AR->isAffine() ||
            !isa<SCEVConstant>(AR->getStepRecurrence(SE)))
          return false;
        Stores.push_back(
            {AR, DLay.getTypeStoreSize(St->getValueOperand()->getType())});
      } else if (I.mayWriteToMemory()) {
        return false; // a call or something else we can't bound
      }
    }
  if (GLoads.empty() || Stores.empty())
    return false;

  // Each store covers [lo, hi): its first and last address, by direction.
  SmallVector<std::pair<const SCEV *, const SCEV *>, 4> Ranges;
  for (auto [AR, Size] : Stores) {
    const SCEV *First = AR->getStart();
    const SCEV *Last = AR->evaluateAtIteration(BTC, SE);
    if (isa<SCEVCouldNotCompute>(Last))
      return false;
    bool Up = !cast<SCEVConstant>(AR->getStepRecurrence(SE))
                   ->getAPInt()
                   .isNegative();
    const SCEV *Lo = Up ? First : Last;
    const SCEV *Hi = Up ? Last : First;
    Hi = SE.getAddExpr(Hi, SE.getConstant(SE.getEffectiveSCEVType(Hi->getType()), Size));
    Ranges.push_back({Lo, Hi});
  }

  formLCSSA(*L, DT, &LI, &SE);

  // The check goes in the old preheader; the loop gets a new one.
  BasicBlock *CheckBB = L->getLoopPreheader();
  BasicBlock *PH = SplitBlock(CheckBB, CheckBB->getTerminator(), &DT, &LI,
                              nullptr, L->getHeader()->getName() + ".ph");
  SCEVExpander Exp(SE, DLay, "fcpu.hoist");
  Instruction *At = CheckBB->getTerminator();
  IRBuilder<> B(At);
  Type *IntPtr = DLay.getIntPtrType(L->getHeader()->getContext());
  SmallPtrSet<GlobalVariable *, 4> Seen;
  Value *Safe = B.getTrue();
  for (auto [Lo, Hi] : Ranges) {
    Value *LoV = B.CreatePtrToInt(Exp.expandCodeFor(Lo, nullptr, At), IntPtr);
    Value *HiV = B.CreatePtrToInt(Exp.expandCodeFor(Hi, nullptr, At), IntPtr);
    Seen.clear();
    for (LoadInst *Ld : GLoads) {
      auto *G = cast<GlobalVariable>(Ld->getPointerOperand());
      if (!Seen.insert(G).second)
        continue;
      Value *GLo = B.CreatePtrToInt(G, IntPtr);
      Value *GHi = B.CreateAdd(
          GLo, ConstantInt::get(IntPtr, DLay.getTypeAllocSize(G->getValueType())));
      Value *Apart = B.CreateOr(B.CreateICmpULE(HiV, GLo), B.CreateICmpULE(GHi, LoV));
      Safe = B.CreateAnd(Safe, Apart);
    }
  }

  // Clone the loop: the clone is the fast version.
  ValueToValueMapTy VMap;
  SmallVector<BasicBlock *, 8> NewBlocks;
  Loop *Fast = cloneLoopWithPreheader(PH, CheckBB, L, VMap, ".fast", &LI, &DT,
                                      NewBlocks);
  remapInstructionsInBlocks(NewBlocks, VMap);
  BasicBlock *Exit = L->getExitBlock();
  BasicBlock *FastExiting = cast<BasicBlock>(VMap[L->getExitingBlock()]);
  for (PHINode &P : Exit->phis()) {
    Value *V = P.getIncomingValueForBlock(L->getExitingBlock());
    if (Value *M = VMap.lookup(V))
      V = M;
    P.addIncoming(V, FastExiting);
  }
  B.CreateCondBr(Safe, Fast->getLoopPreheader(), PH);
  At->eraseFromParent();
  DT.changeImmediateDominator(PH, CheckBB);
  DT.changeImmediateDominator(Fast->getLoopPreheader(), CheckBB);
  DT.changeImmediateDominator(Exit, CheckBB);

  // In the fast copy nothing the loop stores can reach the globals.
  Instruction *FastPHEnd = Fast->getLoopPreheader()->getTerminator();
  for (LoadInst *Ld : GLoads)
    cast<LoadInst>(VMap[Ld])->moveBefore(FastPHEnd);
  SE.forgetLoop(L);
  return true;
}

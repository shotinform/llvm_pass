#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/LoopPeel.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SizeOpts.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include <algorithm>

using namespace llvm;

namespace {

struct OurTCE : public FunctionPass {
  static char ID;
  OurTCE() : FunctionPass(ID) {}


  Instruction* getTailRecursiveInstruction(Function* F){

    if (!F->getReturnType()->isVoidTy())
      return nullptr;

    for (auto &BB : *F){
      for (auto &I : BB) {
        if (auto *CI = dyn_cast<CallInst>(&I)){

          // check self recursion
          if (CI->getCalledFunction() != F)
            continue;

          // case 1: next instruction is return
          auto *nextInstruction = CI->getNextNode();
          if (isa<ReturnInst>(nextInstruction))
            return CI;
          // case 2: next instruction is unconditional branch to basic block which first instruction is return
          else if (auto *BI = dyn_cast<BranchInst>(CI->getNextNode())){
            if (BI->isUnconditional()){
              BasicBlock* Successor = BI->getSuccessor(0);

              if (isa<ReturnInst> (&Successor->front())){
                return CI;
              }
            }
          }
        }
      }
    }

    return nullptr;
  }

  void transformToLoop(Function* F){

  }


  bool runOnFunction(Function& F) override {

    bool codeChanged = false;

    if (Instruction* CallInstruction = getTailRecursiveInstruction(&F)){
      transformToLoop(&F);
      codeChanged = true;
    }

    return codeChanged;
  }
};
}

char OurTCE::ID = 0;
static RegisterPass<OurTCE> X("our-lte-pass", "Our tail call elimination pass.");
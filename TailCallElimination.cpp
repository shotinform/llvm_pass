#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
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

  void transformToLoop(Function* F, Instruction* CallInstruction) {
    IRBuilder<> Builder(F->getContext());

    // Create a new loop header block after the entry block
    BasicBlock *EntryBB = &F->getEntryBlock();
    BasicBlock *LoopHeaderBB = BasicBlock::Create(F->getContext(), "loop_header", F, EntryBB->getNextNode());

    // Move all instructions from the entry block to the new loop header block
    LoopHeaderBB->splice(LoopHeaderBB->end(), EntryBB);

    // Add an unconditional branch from the entry block to the new loop header block
    Builder.SetInsertPoint(EntryBB);
    Builder.CreateBr(LoopHeaderBB);

    // Insert the loop back block just before the return
    Builder.SetInsertPoint(CallInstruction);
    CallInst *CI = cast<CallInst>(CallInstruction);
    for (unsigned i = 1; i < CI->getNumOperands(); ++i) {
      Argument* Arg = F->getArg(i-1);
      Value* ArgVal = CI->getOperand(i);
      Builder.CreateStore(ArgVal, Arg);
    }
    CallInstruction->getNextNode()->eraseFromParent();
    Builder.CreateBr(LoopHeaderBB);

    CallInstruction->eraseFromParent();
  }


  bool runOnFunction(Function& F) override {

    bool codeChanged = false;

    if (Instruction* CallInstruction = getTailRecursiveInstruction(&F)){
      transformToLoop(&F, CallInstruction);
      codeChanged = true;
    }

    return codeChanged;
  }
};
}

char OurTCE::ID = 0;
static RegisterPass<OurTCE> X("our-tce-pass", "Our tail call elimination pass.");
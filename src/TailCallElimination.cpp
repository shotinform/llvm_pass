#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/IRBuilder.h"
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <vector>

using namespace llvm;

namespace {

struct OurTCE : public FunctionPass {
  static char ID;
  OurTCE() : FunctionPass(ID) {}

  bool checkWithLoadAndStore(CallInst* CI){
    if (auto *Store = dyn_cast<StoreInst>(CI->getNextNode())) {
      if (auto *BI = dyn_cast<BranchInst>(Store->getNextNode())) {
        if (BI->isUnconditional()) {
          BasicBlock *Successor = BI->getSuccessor(0);
          Instruction *FirstInstInSuccessor = &Successor->front();
          if (auto *Load = dyn_cast<LoadInst>(FirstInstInSuccessor)) {
            if (Store->getPointerOperand() == Load->getPointerOperand()) {
              Instruction *NextInst = Load->getNextNode();

              if (isa<ReturnInst>(NextInst)) {
                return true;
              }
            }
          }
        }
      }
    }
    return false;
  }

  Instruction* getTailRecursiveInstruction(Function* F){

    //if (!F->getReturnType()->isVoidTy())
    //  return nullptr;

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
          // case 3: Multiple return values non-void, then have store and load
          else if (checkWithLoadAndStore(CI)){
            return CI;
          }
        }
      }
    }

    return nullptr;
  }

  void createLoopBeginBlock(Function* F){
    IRBuilder<> Builder(F->getContext());

    // Create a new loop header block after the entry block
    BasicBlock *EntryBB = &F->getEntryBlock();
    LoopBeginBlock = BasicBlock::Create(F->getContext(), "loop_start", F, EntryBB->getNextNode());

    // get mapping between alloca and function args
    for (auto it = EntryBB->begin(), end = EntryBB->end(); it != end; ++it) {
      Instruction &I = *it;
      if (isa<StoreInst>(&I)) {
        // Check if the 0-th operand of the store is one of the function arguments
        Value *PointerOperand = I.getOperand(0);
        for (Argument &Arg : F->args()) {
          if (PointerOperand == &Arg) {
            ArgToAllocaMap[&Arg] = I.getOperand(1);
          }
        }
      }
    }

    // dummy terminator
    Builder.SetInsertPoint(LoopBeginBlock);
    Builder.CreateBr(LoopBeginBlock);

    std::vector<Instruction*> InstructionsToMove;

    // Move all non alloca and arguments store instructions
    for (Instruction &I : *EntryBB) {
      if (isa<AllocaInst>(&I)) {
        continue;
      }
      else if (isa<StoreInst>(&I) && ArgToAllocaMap.find(I.getOperand(0)) != ArgToAllocaMap.end()){
        continue;
      }
      InstructionsToMove.push_back(&I);
    }

    for (Instruction* I : InstructionsToMove)
      I->moveBefore(LoopBeginBlock->getTerminator());

    LoopBeginBlock->getTerminator()->eraseFromParent();

    // Add an unconditional branch from the entry block to the new loop header block
    Builder.SetInsertPoint(EntryBB);
    Builder.CreateBr(LoopBeginBlock);

  }

  void transformToLoop(Function* F, Instruction* CallInstruction) {

    if (LoopBeginBlock == nullptr)
      createLoopBeginBlock(F);

    IRBuilder<> Builder(F->getContext());

    // Insert the loop back block just before the return
    Builder.SetInsertPoint(CallInstruction);
    CallInst *CI = cast<CallInst>(CallInstruction);
    for (unsigned i = 0; i < CI->getNumOperands() - 1; ++i) {
      Argument *Arg = F->getArg(i);
      Value *ArgVal = CI->getOperand(i);
      Builder.CreateStore(ArgVal, ArgToAllocaMap[Arg]);
    }
    Builder.CreateBr(LoopBeginBlock);

    CallInstruction->getParent()->getTerminator()->eraseFromParent();
    CallInstruction->getNextNode()->eraseFromParent();
    CallInstruction->eraseFromParent();
  }


  bool runOnFunction(Function& F) override {

    bool codeChanged = false;
    LoopBeginBlock = nullptr;
    ArgToAllocaMap.clear();

    while (Instruction* CallInstruction = getTailRecursiveInstruction(&F)){
      std::cout << "Usao" << std::endl;
      transformToLoop(&F, CallInstruction);
      codeChanged = true;
    }

    return codeChanged;
  }

private:
  BasicBlock* LoopBeginBlock;
  std::unordered_map<Value*, Value*> ArgToAllocaMap;

};

}

char OurTCE::ID = 0;
static RegisterPass<OurTCE> X("our-tce-pass", "Our tail call elimination pass.");
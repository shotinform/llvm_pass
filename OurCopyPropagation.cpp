#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>

using namespace llvm;

namespace {

struct CopyPropagationPass : public FunctionPass {
  static char ID;

  CopyPropagationPass() : FunctionPass(ID) {}

  void checkChange(StoreInst* SI){

    for (auto it = VariablesMap.begin(); it != VariablesMap.end(); ) {
      Value *key = it->first;
      Value *mappedValue = it->second;

      if (key == SI->getPointerOperand() || mappedValue == SI->getPointerOperand()) {
        it = StoreMap.erase(it);
      } else {
        ++it;
      }
    }
  }

  bool shouldSave(StoreInst* SI){
    if (isa<Constant>(SI->getValueOperand())){
      return false;
    }

    if (VariablesMap.find(SI->getValueOperand()) != VariablesMap.end())
      return true;
    return false;
  }

  void save(StoreInst* SI){
    StoreMap[SI->getPointerOperand()] = VariablesMap[SI->getValueOperand()];
  }

  bool tryExchange(LoadInst* LI){
    if (StoreMap.find(LI->getPointerOperand()) != StoreMap.end()) {
      LI->setOperand(0, StoreMap[LI->getPointerOperand()]);
      return true;
    }
    return false;
  }


  bool copyPropagation(Function &F) {
    bool changed = false;
    VariablesMap.clear();

    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *SI = dyn_cast<StoreInst>(&I)) {

          checkChange(SI);
          if (shouldSave(SI))
            save(SI);
        }
        else if (auto* LI  = dyn_cast<LoadInst>(&I)){

          changed = tryExchange(LI) ? true : false;
          VariablesMap[&I] = LI->getPointerOperand();
        }
      }
    }

    return changed;
  }

  bool runOnFunction(Function &F) override {

    return copyPropagation(F);
  }

private:
  std::unordered_map<Value*, Value*> VariablesMap;
  std::unordered_map<Value*, Value*> StoreMap;
};

} // namespace

char CopyPropagationPass::ID = 0;
static RegisterPass<CopyPropagationPass> X("our-copy-propagation-pass", "Copy Propagation Pass", false, false);

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
#include <unordered_set>
#include <queue>

using namespace llvm;

namespace {

struct GlobalCopyPropagationPass : public FunctionPass {
  static char ID;

  GlobalCopyPropagationPass() : FunctionPass(ID) {}

  void calculateCPInAndCPOut(Function& F) {

    std::queue<BasicBlock *> WorkList;
    for (auto &BB: F) {
      CPIn[&BB] = std::unordered_set<Value*, Value*>();
      CPOut[&BB] = std::unordered_set<Value*, Value*>();
    }

    WorkList.push(&F.front());

    while (!WorkList.empty()) {
      BasicBlock *BB = WorkList.front();
      WorkList.pop();

      // Ažuriraj CPin skup na osnovu svih prethodnika
      ValuePairSet NewCPin;
      bool isFirstPred = true;
      for (auto Pred = pred_begin(BB), E = pred_end(BB); Pred != E; ++Pred) {
        BasicBlock *PredBB = *Pred;
        if (isFirstPred) {
          NewCPin = CPout[PredBB];
          isFirstPred = false;
        } else {
          // Presek sa postojećim skupom
          ValuePairSet Intersection;
          for (const auto &pair : NewCPin) {
            if (CPout[PredBB].count(pair)) {
              Intersection.insert(pair);
            }
          }
          NewCPin = Intersection;
        }
      }

    }

  }


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

    calculateCPInAndCPOut(F);
    bool changed = false;

    for (auto &BB : F) {
      StoreMap = {};
      VariablesMap.clear();

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

    VariablesMap.clear();
    StoreMap.clear();
    CPIn.clear();
    CPOut.clear();

    return copyPropagation(F);
  }

private:
  std::unordered_map<Value*, Value*> VariablesMap;
  std::unordered_map<Value*, Value*> StoreMap;
  std::unordered_map<BasicBlock*, std::unordered_set<Value*, Value*>> CPIn;
  std::unordered_map<BasicBlock*, std::unordered_set<Value*, Value*>> CPOut;
};

} // namespace

char GlobalCopyPropagationPass::ID = 0;
static RegisterPass<GlobalCopyPropagationPass> X("our-global-copy-propagation-pass", "Global Copy Propagation Pass", false, false);

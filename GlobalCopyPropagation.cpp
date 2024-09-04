#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
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

  std::unordered_map<Value*, Value*> intersectMaps(
      const std::unordered_map<llvm::Value*, llvm::Value*>& map1,
      const std::unordered_map<llvm::Value*, llvm::Value*>& map2) {

    std::unordered_map<llvm::Value*, llvm::Value*> intersection;
    for (const auto& pair : map1) {
      llvm::Value* key = pair.first;
      llvm::Value* value = pair.second;

      auto it = map2.find(key);
      if (it != map2.end() && it->second == value) {
        intersection[key] = value;
      }
    }

    return intersection;
  }

  std::unordered_map<Value*, Value*> copyAndKill(BasicBlock& BB, std::unordered_map<Value*, Value*> StoreMap){
    for (auto &I : BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        std::cout << "Pre checkChanhe" << std::endl;
        checkChange(SI, StoreMap);
        std::cout << "Nakon checkChanhe" << std::endl;
        if (shouldSave(SI)) {
          std::cout << "Pre shouldSave" << std::endl;
          save(SI, StoreMap);
        }
      }
    }
    return StoreMap;
  }



  void calculateCPInAndCPOut(Function& F) {

    std::queue<BasicBlock *> WorkList;
    std::unordered_map<Value*, Value*> tmp;
    bool changed;

    WorkList.push(&F.front());
    CPIn[&F.front()] = std::unordered_map<Value*, Value*>();

    std::cout << "Pre petlje" << std::endl;

    while (!WorkList.empty()) {
      BasicBlock *BB = WorkList.front();
      WorkList.pop();
      changed = false;

      bool firstPred = true;
      std::cout << "Pre prethodnika" << std::endl;
      for (auto* predBB : predecessors(BB)){
        if (firstPred){
          CPIn[BB] = CPOut[predBB];
          firstPred = false;
        }
        else {
          CPIn[BB] = intersectMaps(CPIn[BB], CPOut[predBB]);
        }
      }

      std::cout << "Pre copyAndKilla" << std::endl;

      tmp = copyAndKill(*BB, CPIn[BB]);
      std::cout << "Nakon copyKill" << std::endl;
      if ((CPOut.find(BB) != CPOut.end()) && (tmp != CPOut[BB])) {
        CPOut[BB] = tmp;
        changed = true;
      }



      if (changed){
        for (llvm::BasicBlock* succ : llvm::successors(BB)) {
          WorkList.push(succ);
        }
      }
    }

  }


  void checkChange(StoreInst* SI, std::unordered_map<Value*, Value*>& StoreMap){

    for (auto it = StoreMap.begin(); it != StoreMap.end(); ) {
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

  void save(StoreInst* SI, std::unordered_map<Value*, Value*>& StoreMap){
    StoreMap[SI->getPointerOperand()] = VariablesMap[SI->getValueOperand()];
  }

  bool tryExchange(LoadInst* LI, std::unordered_map<Value*, Value*>& StoreMap){
    Value* currentOperand = LI->getPointerOperand();

    while (StoreMap.find(currentOperand) != StoreMap.end()) {
      currentOperand = StoreMap[currentOperand];
    }
    if (currentOperand != LI->getPointerOperand()) {
      LI->setOperand(0, currentOperand);
      return true;
    }

    return false;
  }

  void mapVariables(Function &F) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (isa<LoadInst>(&I)) {
          VariablesMap[&I] = I.getOperand(0);
        }
      }
    }
  }


  bool copyPropagation(Function &F) {

    std::unordered_map<Value*, Value*> StoreMap;
    mapVariables(F);
    std::cout << "Pre CPIn CPOut" << std::endl;
    calculateCPInAndCPOut(F);
    std::cout << "Pre CPIn CPOut" << std::endl;
    bool changed = false;

    for (auto &BB : F) {
      StoreMap = CPIn[&BB];

      for (auto &I : BB) {
        if (auto *SI = dyn_cast<StoreInst>(&I)) {
          checkChange(SI, StoreMap);
          if (shouldSave(SI))
            save(SI, StoreMap);
        }
        else if (auto* LI  = dyn_cast<LoadInst>(&I)){
          changed = tryExchange(LI, StoreMap) ? true : false;
        }
      }
    }

    return changed;
  }

  bool runOnFunction(Function &F) override {

    VariablesMap.clear();
    CPIn.clear();
    CPOut.clear();

    return copyPropagation(F);
  }

private:
  std::unordered_map<Value*, Value*> VariablesMap;
  std::unordered_map<BasicBlock*, std::unordered_map<Value*, Value*>> CPIn;
  std::unordered_map<BasicBlock*, std::unordered_map<Value*, Value*>> CPOut;
};

} // namespace

char GlobalCopyPropagationPass::ID = 0;
static RegisterPass<GlobalCopyPropagationPass> X("our-global-copy-propagation-pass", "Global Copy Propagation Pass", false, false);

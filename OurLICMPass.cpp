#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
struct OurLICM : public FunctionPass {
  static char ID;
  OurLICM() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    errs() << "Hello from LICM pass.";
    return false;
  }
};
}

char OurLICM::ID = 0;
static RegisterPass<OurLICM> X("our-licm-pass", "Loop invariant computation and code motion");
//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

#include "llvm/IR/Value.h"
#include <map>
#include <string>
#include <deque>

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

bool setContains(std::set<Value *>& set, Value *element) {
  return set.find(element) != set.end();
}

void setUnion(std::set<Value *>& lhs, std::set<Value *>& rhs) {
  lhs.insert(rhs.begin(), rhs.end());
}

void setDiff(std::set<Value *>& lhs, std::set<Value *>& rhs) {
  for (Value *v : rhs) {lhs.erase(v);}
}

std::string set2str(std::set<Value *>& set) {
  std::string result;
  for (auto it = set.begin(); it != set.end(); ++it) {
      if (!result.empty()) {
          result += " ";
      }
      result += (*it)->getName().str(); 
  }
  return result;
}

// This method implements what the pass does
void visitor(Function &F) {
  //auto functionName = F.getName();
  //auto argCount = F.arg_size();

  struct BlockInfo {
    std::set<Value *> ueVar, varKill, liveOut;
  };

  std::map<BasicBlock *, BlockInfo> blockInfo;
  
  std::deque<BasicBlock *> workList;

  for (auto& basicBlock : F) {
    //errs() << basicBlock.getName() << "\n";
    for (auto& ins : basicBlock) {
      switch (ins.getOpcode()) {
        case Instruction::Load: {
          llvm::LoadInst *loadInst = llvm::dyn_cast<llvm::LoadInst>(&ins);
          llvm::Value *operand = loadInst->getPointerOperand();
          if (!setContains(blockInfo[&basicBlock].varKill, operand)) {
            blockInfo[&basicBlock].ueVar.insert(operand);
          } 
        } break;
        case Instruction::Store: {
          llvm::StoreInst *storeInst = llvm::dyn_cast<llvm::StoreInst>(&ins); 
          llvm::Value *operand = storeInst->getPointerOperand();
          blockInfo[&basicBlock].varKill.insert(operand);
        } break;
      default:
        break;
      }
    }
    workList.push_back(&basicBlock);
  }

  while (!workList.empty()) {
    BasicBlock *block = workList.front();
    workList.pop_front();
    std::set<Value *> newLiveOut;
    for (BasicBlock *succ: successors(block)) {
      std::set<Value *> temp = blockInfo[succ].liveOut;
      setDiff(temp, blockInfo[succ].varKill);
      setUnion(temp ,blockInfo[succ].ueVar);
      setUnion(newLiveOut, temp);
    }
    if (blockInfo[block].liveOut != newLiveOut) {
      for (BasicBlock *pred: predecessors(block)) {
        if (std::find(workList.begin(), workList.end(), pred) == workList.end()) {
          workList.push_back(pred);
        }
      }
    }
    blockInfo[block].liveOut = newLiveOut;
  }
  
  for (auto& basicBlock : F) {
    errs() << formatv("----- {0} -----", basicBlock.getName()) << "\n";
    errs() << formatv("UEVAR: {0}", set2str(blockInfo[&basicBlock].ueVar)) << "\n";
    errs() << formatv("VARKILL: {0}", set2str(blockInfo[&basicBlock].varKill)) << "\n";
    errs() << formatv("LIVEOUT: {0}", set2str(blockInfo[&basicBlock].liveOut)) << "\n";
  }
}

// New PM implementation
struct HelloWorld : PassInfoMixin<HelloWorld> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    visitor(F);
    return PreservedAnalyses::all();
  }

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hello-world") {
                    FPM.addPass(HelloWorld());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}

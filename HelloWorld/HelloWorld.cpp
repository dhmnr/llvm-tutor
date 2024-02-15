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

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

struct Operation {
  std::string opStr;

  Operation(int left, int code, int right) {
    this->opStr = formatv("{0} {1} {2}", left, code, right).str();
  }

  Operation(llvm::Value* left) {
   this->opStr = formatv("{0}", left).str();
  }

  bool operator==(const Operation& other) const {
    return opStr == other.opStr;
  }

  bool operator<(const Operation& other) const {
    return opStr < other.opStr;
  }

  
};

class VNHashTable {
  private:
    std::map<Operation, int> vn_data;
    int v;
  public:
    VNHashTable() : v(1) {}

    int insert(Operation op) {
      int r = v;
      vn_data[op] = v;
      v++;
      return r; // vn_data[op];
    }
    void insert(Operation op, int n) {
      vn_data[op] = n;
    }
    int getVN(Operation op) {
      if (vn_data.count(op) == 1)
        return vn_data[op];
      else
        return -1;
    }
    std::string getContentsString() {
      std::string contents;
      for (const auto& pair : vn_data) {
        contents += "HT " + pair.first.opStr +
                    " : " + std::to_string(pair.second) + "\n";
    }
    return contents;
  }
};

bool isIn(std::set<std::string> Ops, std::string ins) {
  auto it = Ops.find(ins);
    if (it != Ops.end())
      return true;
    else
      return false;
}

// This method implements what the pass does
void visitor(Function &F) {
  //auto functionName = F.getName();
  //auto argCount = F.arg_size();
  std::set<std::string> arthOps = {"add", "sub", "mul", "udiv", "sdiv"};
  std::set<std::string> lsOps = {"load", "store"};
  VNHashTable vnht;  

  for (auto& basicBlock : F) {
    // errs() << basicBlock.getName() << "\n";
    for (auto& ins : basicBlock) {
      if (isIn(arthOps, ins.getOpcodeName())) {
        bool isRed = false;
        Value *op0 = ins.getOperand(0);
        Value *op1 = ins.getOperand(1);
        Operation L(op0), R(op1), T(&ins);
        int vL = vnht.getVN(L);
        if (vL == -1) {
          vL = vnht.insert(L);
          //errs() << formatv("Insert {0}->{1}", L.opStr, l ) << "\n";
        }
        int vR = vnht.getVN(R);          
        if (vR == -1) {
          vR = vnht.insert(R);
          //errs() << formatv("Insert {0}->{1}", R.opStr, r ) << "\n";
        }
        Operation LopR(vL, ins.getOpcode(), vR);
        int vLopR = vnht.getVN(LopR);
        if (vLopR != -1) {
          //errs() << formatv("Insert {0}->{1}", LopR.opStr, vLopR ) << "\n";
          isRed = true;
        }
        else {
          isRed = false;
          vLopR = vnht.insert(LopR);
          //errs() << formatv("Insert {0}->{1}", LopR.opStr, lor ) << "\n";
          //errs() << formatv("{0}", vnht.getContentsString()) << "\n";
        }
        vnht.insert(T, vLopR);
        //errs () << formatv("To:{0} Op0:{1} OpCode:{2} Op1:{3} Ins:{4}", &ins ,op0, ins.getOpcode(), op1, ins) << "\n";
        if (isRed)  
          errs () << formatv("{0, -40} {1} = {2} {3} {4} (redundant)", ins, vLopR, vL, ins.getOpcodeName(), vR) << "\n";
        else
          errs () << formatv("{0, -40} {1} = {2} {3} {4}", ins, vLopR, vL, ins.getOpcodeName(), vR) << "\n";

      }
      else if (isIn(lsOps, ins.getOpcodeName())) {
        Value *op0 = ins.getOperand(0);
        Operation L(op0);
        Operation T = (ins.getNumOperands() == 1) ? Operation(&ins) : Operation(ins.getOperand(1));
        // if (ins.getNumOperands() == 1)
        //   auto T = Operation(&ins);
        // else
        //   auto T = Operation(ins.getOperand(1));
        
        int vL = vnht.getVN(L);
        //errs() << formatv("{0}", vnht.getContentsString()) << "\n";
        if (vL == -1) {
          vL = vnht.insert(L);
          vnht.insert(T, vL);
          //errs() << formatv("Insert {0}->{1}", L.opStr, vL) << "\n";
          //errs() << formatv("Insert {0}->{1}", T.opStr, vL) << "\n";
        } else {
          vnht.insert(T, vL);
          //errs() << formatv("Insert {0}->{1}", T.opStr, vL) << "\n";
        }
        //errs () << formatv("To:{0} Op0:{1} OpCode:{2} Ins:{3}", T.opStr ,op0, ins.getOpcode(), ins) << "\n";
        errs () << formatv("{0, -40} {1} = {2}",ins , vL , vL) << "\n";
      }
    }
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

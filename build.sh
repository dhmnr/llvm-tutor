#!/bin/bash
export LLVM_DIR=/lib/llvm-17
mkdir build
cd build
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..
make
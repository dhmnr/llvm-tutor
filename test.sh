#!/bin/bash

clang -c -emit-llvm -fno-discard-value-names -O0 demo.cpp -odemo.bc
opt -load-pass-plugin build/lib/libHelloWorld.so -passes=hello-world -disable-output demo.bc
#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <demo.cpp>"
    exit 1
fi

DEMO_CPP="$1"
OUTPUT_BC="${DEMO_CPP%.c}.bc"

clang -fno-discard-value-names -c -emit-llvm -O0 "$DEMO_CPP" -o "$OUTPUT_BC"
opt -load-pass-plugin build/lib/libHelloWorld.so -passes=hello-world -disable-output "$OUTPUT_BC"

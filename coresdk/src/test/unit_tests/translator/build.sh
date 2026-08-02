#!/bin/bash
# Link libclang library location
# -I: Location of clang-c/Index.h
# -L: Location of libclang.so
# -lclang: link against libclang
# -Wl,-rpath: find libclang.so at runtime
clang++ unit_test_parser.cpp -o unit_test_parser \
    -I/usr/lib/llvm-18/include \
    -L/usr/lib/llvm-18/lib \
    -lclang \
    -Wl,-rpath,/usr/lib/llvm-18/lib
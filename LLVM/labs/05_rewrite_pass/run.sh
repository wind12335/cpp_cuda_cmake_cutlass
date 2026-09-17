#!/bin/bash
# 第二个 Pass：改写 addf %x,%x → mulf %x,2.0（详见 L05）
set -e
cd "$(dirname "$0")"
mkdir -p build && cd build
[ -f my-opt-rw ] || {
  cmake .. -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm \
           -DMLIR_DIR=/usr/lib/llvm-15/lib/cmake/mlir \
           -DCMAKE_BUILD_TYPE=Release > /dev/null
  make -j8 2>&1 | tail -1
}
./my-opt-rw ../test.mlir --mul-to-add 2>/dev/null

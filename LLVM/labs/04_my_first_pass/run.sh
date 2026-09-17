#!/bin/bash
# 第一个 Pass：抄写（详见 L04）
set -e
cd "$(dirname "$0")"
mkdir -p build && cd build
[ -f my-opt ] || {
  cmake .. -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm \
           -DMLIR_DIR=/usr/lib/llvm-15/lib/cmake/mlir \
           -DCMAKE_BUILD_TYPE=Release > /dev/null
  make -j8 2>&1 | tail -1
}
echo "══════ 自己的 Pass --print-op-names ══════"
./my-opt ../test.mlir --print-op-names 2>&1 | head -6
echo "══════ 和官方 Pass 串联 ══════"
./my-opt ../test.mlir --print-op-names --canonicalize 2>&1 | tail -8

#!/bin/bash
# Linalg 三连：降循环 / 自动tiling / tensor路线（详见 L06）
set -e
cd "$(dirname "$0")"
echo "══════ ① 一行 matmul → 三层循环 ══════"
mlir-opt-15 --convert-linalg-to-loops matmul_memref.mlir | head -16
echo "══════ ② 自动 tiling 2x2x2（CUTLASS GemmShape 自动版） ══════"
mlir-opt-15 --linalg-tile="tile-sizes=2,2,2" matmul_memref.mlir | sed -n '3,14p'
echo "══════ ③ tensor 先 bufferize 再降（跑通不崩的正路） ══════"
mlir-opt-15 --one-shot-bufferize --convert-linalg-to-loops matmul_tensor.mlir | grep -c "scf.for" | xargs echo "scf.for 循环数:"

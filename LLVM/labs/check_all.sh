#!/bin/bash
# 一键跑全部 6 个实验（首次会编译两个 Pass，约2分钟）
cd "$(dirname "$0")"
for d in 01_llvm_pipeline 02_operation_tree 03_canonicalize 04_my_first_pass 05_rewrite_pass 06_linalg_matmul; do
  echo ""
  echo "════════════════════ $d ════════════════════"
  bash "$d/run.sh" || echo "❌ $d 失败"
done
echo ""
echo "全部完成 ✅"

#!/bin/bash
# 走完编译器三层：clang前端 → opt中端 → llc后端 → 可执行
set -e
cd "$(dirname "$0")"
clang-15 -S -emit-llvm -O0 -Xclang -disable-O0-optnone matmul.c -o matmul_O0.ll
opt-15 -S -O2 matmul_O0.ll -o matmul_O2.ll
# ⚠️ 踩坑：llc 默认生成非 PIC 代码，Ubuntu 默认 PIE 链接，会报
#   "relocation R_X86_64_32 ... can not be used when making a PIE object"
# 解法：llc 加 -relocation-model=pic
llc-15 -O2 -relocation-model=pic matmul_O2.ll -o matmul_O2.s
clang-15 matmul_O2.s -o matmul_bin
./matmul_bin
echo "向量化确认：$(grep -c 'vector.body' matmul_O2.ll) 处 vector.body（>0 说明 O2 自动向量化生效）"

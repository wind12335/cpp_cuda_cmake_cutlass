#!/bin/bash
# 浮点陷阱：整数 +0 被消，浮点 +0.0 不被消（IEEE 754 负零/NaN）
set -e
cd "$(dirname "$0")"
echo "══════ 整数版：+0 应消失 ══════"
mlir-opt-15 --canonicalize canonicalize_int.mlir
echo "══════ 浮点版：+0.0 原样保留（陷阱！） ══════"
mlir-opt-15 --canonicalize canonicalize_float.mlir
echo "══════ CSE：重复乘法只留一个 ══════"
mlir-opt-15 --cse cse_demo.mlir

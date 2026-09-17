#!/bin/bash
# 一切皆 Operation：漂亮格式 vs 泛型格式 vs 方言列表
set -e
cd "$(dirname "$0")"
echo "══════ ① 原样解析（验证语法） ══════"
mlir-opt-15 smoke_test.mlir > /dev/null && echo "解析 OK"
echo "══════ ② 脱掉语法糖（一切皆 Operation 的证据） ══════"
mlir-opt-15 --mlir-print-op-generic generic_demo.mlir | head -12
echo "══════ ③ 本机注册的方言（前10个） ══════"
mlir-opt-15 --show-dialects | head -10

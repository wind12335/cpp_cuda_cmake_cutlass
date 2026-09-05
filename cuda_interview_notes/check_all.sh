#!/usr/bin/env bash
# 一键检查: demos 编译 + notes-v2 二进制可用
set -u
cd "$(dirname "$0")"
ARCH=sm_89
pass=0; fail=0

echo "== 1) demos 编译 =="
for f in demos/*.cu; do
    if nvcc -std=c++17 -O2 -arch=$ARCH "$f" -o /dev/null 2>/tmp/cuda_notes_err.txt; then
        echo "PASS  $f"; pass=$((pass+1))
    else
        echo "FAIL  $f"; cat /tmp/cuda_notes_err.txt; fail=$((fail+1))
    fi
done

echo "== 2) LeetCUDA notes-v2 锚点 =="
BIN=~/workspace/LeetCUDA/kernels/interview/notes_v2_cute_sm89.bin
if [ -x "$BIN" ]; then
    echo "PASS  notes_v2_cute_sm89.bin 存在 ($(du -h "$BIN" | cut -f1))，直接运行可出验证表"
    pass=$((pass+1))
else
    echo "WARN  未找到已编译 bin，需要时: cd ~/workspace/LeetCUDA/kernels/interview && ./build.sh --arch $ARCH"
fi

echo "-----"
echo "通过 $pass, 失败 $fail"
[ "$fail" -eq 0 ]

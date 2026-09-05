#!/usr/bin/env bash
# 一键编译检查全部 OS/网络 demo: bash check_all.sh
set -u
cd "$(dirname "$0")"
pass=0; fail=0

for f in demos/*.c demos/*.cpp; do
    [ -e "$f" ] || continue
    if g++ -O2 -pthread -o /dev/null "$f" 2>/tmp/os_net_err.txt; then
        echo "PASS  $f"; pass=$((pass+1))
    else
        echo "FAIL  $f"; cat /tmp/os_net_err.txt; fail=$((fail+1))
    fi
done

for f in demos/*.cu; do
    [ -e "$f" ] || continue
    if nvcc -std=c++17 -O2 -arch=sm_89 -o /dev/null "$f" 2>/tmp/os_net_err.txt; then
        echo "PASS  $f"; pass=$((pass+1))
    else
        echo "FAIL  $f"; cat /tmp/os_net_err.txt; fail=$((fail+1))
    fi
done

echo "-----"
echo "通过 $pass, 失败 $fail"
[ "$fail" -eq 0 ]

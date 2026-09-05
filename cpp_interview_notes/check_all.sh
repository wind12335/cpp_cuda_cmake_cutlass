#!/usr/bin/env bash
# 一键编译检查: bash check_all.sh
set -u
cd "$(dirname "$0")"
pass=0; fail=0
for f in $(find . -name '*.cpp' | sort); do
    if g++ -std=c++17 -O2 -pthread -Wall -o /dev/null "$f" 2> /tmp/cpp_notes_err.txt; then
        echo "PASS  $f"; pass=$((pass+1))
    else
        echo "FAIL  $f"; cat /tmp/cpp_notes_err.txt; fail=$((fail+1))
    fi
done
echo "-----"
echo "通过 $pass, 失败 $fail"
[ "$fail" -eq 0 ]

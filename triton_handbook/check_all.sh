#!/bin/bash
# 一键跑全部 triton 实验(在你的 uv 环境里)
set -e
cd "$(dirname "$0")"
PY=${PYTHON:-$HOME/.venv/bin/python}
[ -x "$PY" ] || PY=python
echo "使用解释器: $PY ($($PY --version 2>&1))"
for f in 01_vector_add.py 02_transpose.py 03_matmul.py 04_fused_softmax.py; do
    echo ""
    echo "════════════════════ $f ════════════════════"
    $PY "$f"
done
echo ""
echo "════════════════════ 05_dump_ir.sh ════════════════════"
bash 05_dump_ir.sh
echo ""
echo "全部完成 ✅"

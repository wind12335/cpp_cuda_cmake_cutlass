#!/bin/bash
# labs/05_dump_ir.sh —— T06 配套: 一键从 Triton 缓存捞出 TTIR/TTGIR/PTX
# 运行: bash 05_dump_ir.sh
set -e
cd "$(dirname "$0")"
PY=${PYTHON:-$HOME/.venv/bin/python}
[ -x "$PY" ] || PY=python

echo "① 先编译一个 kernel(产生缓存)..."
$PY 01_vector_add.py > /dev/null

echo "② 定位缓存目录(TRITON_CACHE_DIR, 默认 ~/.triton/cache)..."
TDIR=$(find ~/.triton/cache -name "*.ttir" 2>/dev/null | head -1 | xargs dirname)
if [ -z "$TDIR" ]; then echo "没找到缓存! 先跑 01_vector_add.py"; exit 1; fi
echo "   $TDIR"
echo "   里面躺着: $(ls "$TDIR" | tr '\n' ' ')"
echo ""

echo "════════ TTIR(块级 MLIR 方言 — LLVM/L02 的预告片) ════════"
grep -E "module|tt\.func|tt\.load|tt\.store|arith\.|make_range" "$TDIR"/*.ttir | head -12
echo ""
echo "════════ TTGIR(线程级 — 块被拆成 warp/layout) ════════"
grep -E "ttg\.|blocked|layout|warps" "$TDIR"/*.ttgir 2>/dev/null | head -8 || echo "(此版本文件名可能不同, ls 看看)"
echo ""
echo "════════ PTX(你 G 系列的老朋友) ════════"
grep -E "\.visible \.entry|ld\.global|st\.global|mma|add\.f" "$TDIR"/*.ptx | head -8
echo ""
echo "完整文件自己慢慢看: less $TDIR/*.ttir"
echo "想看编译每一遍 Pass 的直播(T06 实操②): MLIR_ENABLE_DUMP=1 $PY 01_vector_add.py 2>&1 | less"

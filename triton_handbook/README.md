# Triton 学习手册（triton_handbook）

> 目标：会**写**算子（LeetCUDA 全套题型）、会**调优**算子（autotune/性能分析）、
> 能看懂 Triton 的**编译链路**（TTIR/TTGIR→LLVM→PTX）——直接作为 LLVM/MLIR 手册的前置。
> 你的 27 个 `triton_learning/` 练习文件是本手册的素材库，逐篇对接。

## 环境（你的机器已就绪，实测通过）

```bash
source ~/.venv/bin/activate        # 你的 uv 虚拟环境
python -c "import torch, triton; print(torch.__version__, triton.__version__)"
# torch 2.11.0+cu129 | triton 3.6.0 | RTX 4060 (sm_89) ✓
```

## 结构

| 章节 | 内容 | 用的你的素材 |
|---|---|---|
| T00 | 总览：学习路线 + LLVM 前置地图 + AMD ISA 要不要学 | — |
| T01 | 编程模型与第一个 kernel（program_id/grid/BLOCK/mask）| triton-add.py |
| T02 | 访存：tl.load/store/mask/stride/二维块/转置 | triton_transpose.py |
| T03 | tl.dot 与矩阵乘（BLOCK_M/N/K，对照 CUTLASS）| triton_general_GEMM.py |
| T04 | autotune 与性能调优（num_warps/num_stages 搜索）| — |
| T05 | 融合 kernel（softmax/swiglu/reduce+atomic）| triton_swiglu/reduce.py |
| T06 | **编译链路：亲手捞出 TTIR/TTGIR/PTX（LLVM 之桥）** | — |

labs/ 里每个实验都可在 4060 上一键复现：`cd labs && bash check_all.sh`

## 和其他手册的关系

```
cuda_handbook(G系列: 手写 CUDA)      ← 你已掌握"手动最优"
        ↓
triton_handbook(T系列: 本手册)        ← 用 Python 写出接近手写的性能
        ↓
LLVM/(L系列: MLIR 编译器)             ← T06 亲眼见过 MLIR 后再学, 事半功倍
cutlass_handbook                      ← T03 的 tiling 概念对照
```

# G07 wmma 张量核心入门 【进阶】

> 用最容易上手的方式触碰 Tensor Core：wmma API（不需要手写 PTX）。
> 学完你就能看懂 LeetCUDA `hgemm/wmma/` 目录的全部代码。

## §G07.1 wmma 的心智模型

**作用**：把"一小块矩阵乘"当成一个整体操作。

```
D(16x16) = A(16x16) × B(16x16) + C(16x16)     ← 一条条 mma 指令算完
```

三个概念：
- **fragment（片段）**：矩阵的一小块"摊在寄存器上"的表示。你**不直接访问** fragment 里的元素，
  只能 load 进去 / mma 计算 / store 出来；
- **三种 frag 角色**：accumulator（累加器，存 C/D）、matrix_a、matrix_b；
- **layout**：A/B 装载时声明内存布局（row_major / col_major）。

## §G07.2 wmma 三段式模板代码

**作用**：所有 wmma kernel 都是这个骨架——load → mma → store。

```cuda
#include <mma.h>
using namespace nvcuda;

#define WMMA_M 16
#define WMMA_N 16
#define WMMA_K 16

__global__ void wmmaKernel(const half* a, const half* b, float* c, int M, int N, int K) {
    // ① 声明三个片段(它们就是一组寄存器的抽象)
    wmma::fragment<wmma::accumulator, WMMA_M, WMMA_N, WMMA_K, float> c_frag;
    wmma::fragment<wmma::matrix_a,    WMMA_M, WMMA_N, WMMA_K, half,  wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b,    WMMA_M, WMMA_N, WMMA_K, half,  wmma::col_major> b_frag;

    wmma::fill_fragment(c_frag, 0.0f);                       // 累加器清零

    // ② 沿 K 维循环: 装载 A/B 片段 → 矩阵乘累加
    for (int k = 0; k < K; k += WMMA_K) {
        wmma::load_matrix_sync(a_frag, a + /*A 的 tile 偏移*/, K);   // 行主序 A: 行距=K
        wmma::load_matrix_sync(b_frag, b + /*B 的 tile 偏移*/, K);   // col_major B: 列距=K
        wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);              // C += A×B
    }

    // ③ 结果写回 global (行距 = N)
    wmma::store_matrix_sync(c + /*C 的 tile 偏移*/, c_frag, N, wmma::mem_row_major);
}
```

**一个 warp 负责一个 16×16 输出 tile**——所以启动时 `block` 里的 warp 数 = 要算几个 tile
（`blockDim.x / 32` 个）。每个 warp 计算自己 tile 在 C 里的行列偏移：`warpRow = (warpId) % tiles`。

## §G07.3 半精度类型 half

**作用**：fp16 数据类型（前置：面试 G21 精度四兄弟）。

```cuda
#include <cuda_fp16.h>
half h = __float2half(3.14f);       // float → half
float f = __half2float(h);          // half → float
// wmma 里 A/B 用 half, 累加器用 float —— "fp16 进, fp32 算账"
```

【坑】half 不能直接用 printf 的 %f——先 `__half2float` 转回 float。

## §G07.4 为什么累加器是 float

**作用**：精度规则。fp16 只有 ~3 位十进制有效数字，连加 16 次（K=16）中间结果就可能丢失精度；
fp32 累加器保证"多次乘加不漂移"。**这是混合精度的铁律：低精度进，高精度算账。**

## §G07.5 与手写 MMA 的关系

| 层级 | 写法 | 学习路径 |
|------|------|---------|
| wmma | 片段抽象 | **本章**——入门 |
| mma PTX | `mma.sync.m16n8k16` 手排寄存器 | LeetCUDA `hgemm/mma/`（有 ldmatrix+swizzle） |
| WGMMA/TMA | Hopper 专属 | 租 H100 后（LeetCUDA `hgemm/wgmma/`） |

面试口径："我从 wmma 入门理解了 frag 三段式，然后读 LeetCUDA 的 mma PTX 版本理解了
寄存器级布局和 swizzle 消 bank conflict，WGMMA 等上了 H100 再深入。"

## ⚠️ 本篇 lab 真实踩坑实录（layout 不匹配 = 学 wmma 的第一道坎）

初版实验结果恒为 1240（= Σk²），排查过程：
1. 先怀疑 GPU 算错 → 打印完整 16×16 输出，发现 GPU 结果**完全符合自己的分块逻辑**；
2. 再查发现：**B 按行主序初始化，却声明 col_major 装载**——wmma 按"列连续"理解数据，
   等于把 B 转置着用；
3. 修正初始化后仍有误差 → 最后发现是 **CPU 对照代码**读了错误的布局（`hb[k*N+j]` vs
   `hb[k+j*K]`），误报错误。

三条教训：①wmma 声明的 layout 必须与数据真实的存储布局一致；②CPU 参照必须用**同一种**
布局解读；③"打印完整输出"是定位这类问题的最快路径。

## 本篇实验

`labs/G07_wmma.cu`——16x16x16 单 tile 的 wmma 矩阵乘，与 CPU 结果对照验证。

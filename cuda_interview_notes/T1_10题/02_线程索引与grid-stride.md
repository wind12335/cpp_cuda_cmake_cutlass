# 02 线程索引计算与 grid-stride loop

## 面试口述版

三层坐标：`threadIdx`（block 内）、`blockIdx`（grid 内）、`blockDim`/`gridDim`（每层大小）。
全局一维索引的标准公式：`idx = blockIdx.x * blockDim.x + threadIdx.x`。
数据量超过线程总数时用 **grid-stride loop**：

```cuda
for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < N; i += gridDim.x * blockDim.x)
```

好处三点：① 网格大小与数据量**解耦**（同一个 kernel 处理任意 N）；② 每个**线程处理多个元素**，
摊薄了索引计算/启动开销；③ 块内相邻线程处理相邻元素，天然保持**合并访存**。
vectorized 版本再把循环步长乘 4（float4 一次搬 16 字节），就是 LeetCUDA 里所有 `-Vec4` 后缀 kernel 的套路。

## 高频追问

- **idx 计算顺序为什么是 `blockIdx.x * blockDim.x + threadIdx.x` 而不是反过来？** 相邻 threadIdx 的元素要相邻——合并访存的根本（联动 04 题）。
- **二维矩阵怎么算？** `row = blockIdx.y * blockDim.y + threadIdx.y; col = ...`，注意 row-major 时 `a[row * N + col]`，行内相邻线程 → 相邻 col → 合并。
- **什么时候不用 grid-stride？** 元素数固定且想"一个线程一个元素"的简单场景；或者每个 block 要做 shared memory 协作时按 block 粒度组织。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读最标准的索引 + vec4 变体 | `LeetCUDA/kernels/elementwise/elementwise.cu`：对比 `elementwise_add_f32_kernel` 与 `elementwise_add_f32x4_kernel`（一个线程算 4 个） |
| 读带 grid-stride 的完整实战 | `interview/notes-v2.cu` Phase 2（ReLU/ElementwiseAdd/Histogram，基础版 + Vec4 版对照） |
| 跑验证 | `./notes_v2_cute_sm89.bin`（看 ReLU / ElemwiseAdd / Vec4 行的 Max Err 全 0） |

## 记忆钩子

"起点 = blockIdx×blockDim + threadIdx；跨步 = grid 总线程数；一步四格用 float4。"

**人话版**：每个线程要算"我负责第几个元素"，公式 = **我所在的 block 编号 × 每 block 线程数 + 我在
block 内的编号**（和二维数组算一维下标一个道理）。数据比线程多时，每个线程处理完一个就**往前跳
全体线程总数**再处理下一个（这就是 grid-stride 循环）；`float4` 是再进一步——一次读 16 字节
（4 个 float 打包读），搬运次数直接除以 4。

# 05 Shared Memory Bank Conflict

## 面试口述版

shared memory 物理上分成 **32 个 bank**，每 bank 宽 **4 字节**，按地址交错的bank = (addr/4) % 32。
一个 warp 同时访问 shared 时，若多个线程访问**不同地址但落在同一 bank**，访问只能**串行化**
（N 个线程同 bank → N-way conflict，延迟 ×N）；同地址的多线程访问反而是广播，不冲突。
两大典型场景与解法：
① **矩阵转置**：从 shared 读转置数据时列访问正好同 bank → 经典解法是 **padding**（每行多加 1 列，
`smem[i][j] = ...` 改成 `smem[i][WIDTH+1]`），把列错开；
② **MMA/WMMA 数据喂给 Tensor Core**：ldmatrix 的加载模式天然撞 bank → **swizzle（异或重排）**，
按位异或打乱存储布局，读写两侧约定同一映射，冲突消失（LeetCUDA hgemm 高级版全在用）。

## 高频追问

- **padding 有代价吗？** 多几列的额外内存和一个取模/常量，几乎免费——所以它是首选方案；swizzle 用于 padding 会破坏 ldmatrix 128-bit 对齐加载的场景。
- **half2/bfloat2 访问怎么算 bank？** 还是按 4 字节粒度算 bank 号，半个 4B 内的两个 half 同 bank 同地址 → 广播不冲突。
- **怎么检测？** `compute-sanitizer --tool racecheck` 会直接报 shared memory bank conflict；ncu 的 Memory Workload Analysis 也有 shared bank 冲突统计。
- **和 CPU false sharing 是一回事吗？** 都属"存储粒度撞车"：bank conflict 是 warp 内 32 线程撞 4B bank（shared），false sharing 是核间撞 64B 缓存行——优化思路都是错开位置（我在 C++ 25 题里讲过 CPU 侧）。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读转置经典实现（含 padding 前后） | `mat-transpose/mat_transpose.cu`（2D tile + shared，注意有没有 padding 变体）+ 同目录 `mat_transpose_cute.cu`（swizzle 版） |
| 读 bank conflict 专题笔记 | `nvidia-nsight/bank_conflicts.md` |
| 读 swizzle 最小示例 | `swizzle/mma_simple_swizzle.cu` + `swizzle/mat_trans_swizzle.cu`，配 `print_swizzle_layout.py` 可视化异或模式 |
| 检测工具 | `compute-sanitizer --tool racecheck ./你的程序` |

## 记忆钩子

"32 个窗口 4 字节宽，排队的全串行；加一列 padding 错开队，异或 swizzle 一劳永逸。"

**人话版**：shared memory 物理上分成 **32 个银行窗口（bank）**，每窗口宽 **4 字节**。
同一 warp 的 32 个线程同时取数时：取**不同窗口** → 并行完成；取**同一窗口的不同地址** → 只能
一个接一个排队（N 个线程同窗口 = 慢 N 倍）；取**同窗口同地址** → 广播，不算冲突。
解法两招：**padding**（每行末尾多垫一格，让下一行整体错开一个窗口）；**swizzle**（写入时按异或
规律把位置打乱，读取时按同样规律取回，冲突消失——MMA 高级版用这招）。

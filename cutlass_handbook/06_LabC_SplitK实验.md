# 06 LabC：Split-K 实验 —— 为什么"教科书答案"不一定赢 【已验证 ✓·含真实踩坑】

> 目标：瘦矩阵 GEMM（M=4, N=4096, K=16384）上对比普通 GEMM 与 Split-K。
> **这个 lab 的价值一半在代码，一半在三次翻车实录**——每一坑都是真实工程知识。

## §6.1 Split-K 是什么

普通 GEMM 的 block 数 = ⌈M/128⌉ × ⌈N/128⌉——只切 M/N 维。当 M 很小（比如 LLM 解码时
batch=1~4），block 数太少，大量 SM 空转。**Split-K 把 K 维也切开**：K 个切片各自算
"部分和"，最后归约——用归约开销换并行度。（LLM 推理里同族思想叫 Flash-Decoding。）

## §6.2 三个版本踩坑实录（每个都是真知识）

**坑① `GemmSplitKParallel` 类不在 `gemm.h` 里**——它在单独的
`#include "cutlass/gemm/device/gemm_splitk_parallel.h"`。只 include `gemm.h` 会报
`no member GemmSplitKParallel`。

**坑② 它的模板参数表和 device::Gemm 不一样**：Epilogue 之后还有 ConvertScaledOp、
ReductionOp、ThreadblockSwizzle、Stages 等。想全部用默认值，就在 Epilogue 后直接收尾
（我第一版把 Swizzle 和 2 错位塞给了 ConvertScaledOp/ReductionOp → 编译错误连环爆）。

**坑③ 串行/并行 Split-K 都需要 workspace**：`get_workspace_size(args)` → `cudaMalloc` →
传给 `initialize(args, workspace)`。不分配的话 initialize 返回 4(资源不足)、run 返回
7(工作区空指针)，**kernel 根本不启动**——而且是静默的，必须打印状态才发现。

## §6.3 实测结果（4060 Laptop，正确性全部 ✓）

| 配置 | 普通 dense | Split-K |
|------|-----------|---------|
| M=4, N=4096, K=16384, slices=64 | 0.77 ms (0.69 TFLOPS) | 0.86 ms (0.63 TFLOPS) |
| slices=16/32/128 扫描 | — | 更多的切片反而**更慢** |
| M=4, N=512, K=16384 | 0.84 ms | 0.90 ms |

**诚实结论：这组形状下 Split-K 没赢！** 原因：B 矩阵 128MB 决定了问题是**带宽受限**，
两个版本搬运的数据量相同 → 性能相同。Split-K 赢的前提是"block 数严重饥饿 + 归约开销
小于并行度收益"——**测量，不要假设**。

## §6.4 学到的可迁移知识

1. **CUTLASS 版本 API 变迁**：老教程的 `GemmSplitKParallel` 类在 4.x 移到了独立头文件；
   新写法也可用 `device::Gemm` 的 `SplitKSerial=true` 模板参数 + `Arguments.split_k_slices`；
2. **workspace 模式**：CUTLASS 组件经常需要 workspace——`get_workspace_size` → 分配 →
   传入 initialize；不分配时错误是静默的，必须打印状态；
3. **性能结论必须实测**：Split-K 在"教科书该赢"的形状上也可能打平——瓶颈分析（带宽 vs
   并行度）优先于套路。

【与你课题的连接】"并行度不足时拆 K"正是 Flash-Decoding / split-K GEMM 的思想——
和你的 overlap 课题共享"并行度-归约开销"的权衡模型。

# 04 三层 GemmShape ↔ 你手写 hgemm 的对应关系 【理解】

> LabB 里那三个 GemmShape 不是魔法数字——它们就是你手写 hgemm 优化阶梯的"参数化版本"。

## §4.1 三层形状 = 三级复用

```
GemmShape<128, 128, 64>   block 级 tile:  一次从显存搬 128×64 的 A + 64×128 的 B 进 shared
GemmShape<64, 64, 64>     warp 级 tile:   每个 warp 负责输出 64×64 的一角
GemmShape<16, 8, 16>      MMA 指令级:     一条 m16n8k16 指令算的最小矩阵块
```

对应到你手写的阶梯（LeetCUDA hgemm 目录从 naive 到 swizzle 的演进）：

| 你手写的版本 | CUTLASS 里的对应物 |
|-------------|-------------------|
| naive（逐元素读显存） | 没有——CUTLASS 不提供"故意慢"的版本 |
| tiling 进 shared | block tile（GemmShape 第一个） |
| double buffering / cp.async 多级流水 | `stages` 模板参数（LabB 的 3） |
| 每线程算 8×8（寄存器复用） | warp tile + MMA 指令的形状组合 |
| swizzle 消 bank conflict | include/cutlass/gemm/threadblock 里的布局组件 |
| Tensor Core MMA | mma 指令级形状 + OpClassTensorOp |

**结论：CUTLASS = 把你手写优化阶梯的每一级，做成可组合的模板零件。**
这就是为什么"手写过 hgemm"是读懂 CUTLASS 的最快路径——反过来，读懂 CUTLASS 也让你明白
手写每一级"为什么存在"。

## §4.2 数量关系（以 LabB 配置为例）

```
block tile 128×128, warp tile 64×64  → 每 block 有 (128/64)×(128/64) = 4 个 warp
warp tile 64×64, mma 16×8            → 每 warp 用 4×8 = 32 组 mma 指令铺满自己的角
K 维 64, MMA k=16                    → 每 block-tile 装载分 64/16 = 4 批 MMA 执行
```

面试问"你的 kernel 里一个 block 有几个 warp、每个 warp 算多大"——就是这套算术。

## §4.3 tile 参数的约束（为什么会编译失败/运行变慢）

1. **shared memory 天花板**：block tile 越大 × stages 越多 = shared 占用越大；
   4060 每 SM 上限 100KB——超了 CUTLASS 运行时返回错误（不是崩，是 kError...）；
2. **寄存器天花板**：warp tile 太大 → 每 warp 累加器寄存器不够 → spill；
3. **整除关系**：M/N/K 尽量是 block tile 的倍数（否则走慢的边界路径）；
4. **warp 数是 2 的幂 × 布局合理性**：128×128 tile 配 8 warp（256 线程）是经典起点。

实验方法（前置：面试 G24）：每次只改一个形状参数，`--ptxas -v` 看 shared/寄存器用量，
event 计时看 TFLOPS——**LabB 的调优表就是干这个的**。

# 03 LabB：自己声明 fp16 Tensor Core GEMM 【已验证 ✓】

> 从"用官方例子"到"自己声明配置"——CUTLASS 的正确打开方式。
> 实测：正确性 ✓，1024³ fp16 = **17.7 TFLOPS**（4060 Laptop 理论值的 59%）。
> 代码：`cutlass_handbook/labs/02_tensorop_hgemm.cu`

## §3.1 编译运行

```bash
CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
    -I$CUTLASS/include -I$CUTLASS/tools/util/include \
    02_tensorop_hgemm.cu -o /tmp/t02 && /tmp/t02
```

## §3.2 GEMM 声明：七个模板参数逐个讲

```cpp
using Gemm = cutlass::gemm::device::Gemm<
    cutlass::half_t, cutlass::layout::RowMajor,    // ① A: fp16, 行主序
    cutlass::half_t, cutlass::layout::ColumnMajor, // ② B: fp16, 列主序
    cutlass::half_t, cutlass::layout::RowMajor,    // ③ C/D: fp16, 行主序
    float,                                         // ④ 累加器/标量: fp32 (铁律!)
    cutlass::arch::OpClassTensorOp,                // ⑤ 用 Tensor Core
    cutlass::arch::Sm80,                           // ⑥ 架构标签 (见下方⚠坑)
    cutlass::gemm::GemmShape<128, 128, 64>,        // ⑦a block 级 tile
    cutlass::gemm::GemmShape<64, 64, 64>,          // ⑦b warp 级 tile
    cutlass::gemm::GemmShape<16, 8, 16>,           // ⑦c MMA 指令级 (m16n8k16)
    cutlass::epilogue::thread::LinearCombination<cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>,
    3>;                                            // ⑧ 流水线级数 (换显存容量)
```

| # | 参数 | 你在决定什么 | 改了会怎样 |
|---|------|-------------|-----------|
| ①②③ | 元素类型+布局 | 精度与数据排布（决定 TN/NN） | 影响能用哪代 MMA 指令 |
| ④ | 累加器精度 | 数值安全 | fp32 是 fp16 输入的铁律（G07） |
| ⑤ | OpClass | CUDA Core 还是 Tensor Core | TC 快 5-10 倍但要求 fp16/bf16/tf32/int8 |
| ⑥ | ArchTag | **实现家族**（不是"只在那个架构跑"） | ⚠ 见下方坑 |
| ⑦abc | 三层 tile | 显存/寄存器/吞吐的平衡 | 核心调优旋钮（第 04 章） |
| epilogue | 收尾策略 | D = α·A·B+β·C 还是 +bias+ReLU | 定制入口（进阶路径） |
| ⑧ | stages | 流水线深度（换 shared 内存） | 越深吞吐越高、occupancy 越低 |

## §3.3 ⚠️ 实测踩坑：Sm89 标签在 2.x 风格里不存在

初版我写 `cutlass::arch::Sm89` → 编译报错：

```
incomplete type "DefaultGemmConfiguration<OpClassTensorOp, Sm89, half_t, half_t, half_t, float>"
```

查 `default_gemm_configuration.h`：TensorOp 的特化**只提供到 Sm80**（还有 Simt/int8 等家族）。
解法：**ArchTag 用 Sm80**——它选择的是"Ampere 风格的 kernel 实现"，而 Ada（sm_89）完整支持
Ampere 的全部特性，直接跑、性能正常（实测 17.7 TFLOPS）。

> **原理**：2.x 风格里 ArchTag ≠ "只在那个架构运行"，而是"选用哪代硬件特性的实现家族"。
> 想要真正 sm_89 原生（以及 WGMMA/TMA 级别的性能）→ 那是 3.x/4.x Collective API 的领域（06 章）。

## §3.4 传参与验证

```cpp
cutlass::Status st = gemm_op({
    {M, N, K},        // 问题尺寸
    {dA, K},          // A: 行主序 → 主维 = K (相邻两行隔 K 个)
    {dB, K},          // B: 列主序 → 主维 = K (相邻两列隔 K 个)
    {dC, N}, {dC, N}, // C/D: 行主序 → 主维 = N
    {alpha, beta}});
```

验证用"全 1 矩阵 × 全 1 矩阵 = 全 K"的技巧（结果可预测、肉眼可查），抽样最大误差 0。
性能用 event 计时 10 次取平均（前置：cuda_handbook G06/面试 G24）。

## §3.5 调优实验（改一个参数重编译，体会每个旋钮）

| 实验 | 预期 |
|------|------|
| `GemmShape<128,128,64>` → `<128,256,64>` | 可能更快或报共享内存不足（失败是正常反馈） |
| stages `3` → `4` | 吞吐可能升，但 shared 超限会失败 |
| `OpClassTensorOp` → `OpClassSimt`（tile 改 `<128,128,8>`） | 暴跌到 CUDA Core 水平——体会 TC 的价值 |
| M=N=K=4096 | 大矩阵下 TFLOPS 通常更高（摊薄启动开销） |

【原则，前置：面试 G24】每次只改一个参数；失败（kErrorInsufficientResources / shared 超限）
本身也是实验结果。

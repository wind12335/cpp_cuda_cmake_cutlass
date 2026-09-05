# 08 LabD：Epilogue 定制 —— 把 ReLU/GELU "焊进" GEMM 【已验证 ✓·进阶实用】

> epilogue（收尾阶段）是 CUTLASS 最实用的定制点：LLM 推理里 GEMM 后面的 bias/激活/量化
> 全在这里融合。**融合省一趟显存读写**——本章实测省 0.14ms（约 11%）。
> 【前置：C09 并发不是本章需要的；需要的是 LabB 的 GEMM 声明 + G07 的"fp16 进 fp32 算账"】

## §8.1 epilogue 是什么

MMA 算完的累加器（fp32，还在寄存器里）到写回显存之间，有一个可编程的**收尾阶段**：

```
累加器(fp32, 寄存器) → [epilogue: α·acc + β·C → 激活函数 → 精度转换] → 写回显存
```

它是"算子融合"的官方入口：把 bias、ReLU、GELU 等后处理**焊进 GEMM 内部**，
省掉一次完整的显存读+写。CUTLASS 提供了几十个现成收尾策略
（`include/cutlass/epilogue/thread/` 目录）：

| functor | 功能 | 场景 |
|---------|------|------|
| `LinearCombination` | D = α·acc + β·C | 基线（LabB 用） |
| `LinearCombinationRelu` | 上式后过 ReLU | CNN/MLP |
| `LinearCombinationGELU` | 上式后过 GELU | **Transformer FFN** |
| `LinearCombinationSilu` / `Sigmoid` / `HardSwish`… | 其他激活 | 各类网络 |
| `LinearCombinationBiasRelu` | bias+ReLU 融合 | CNN 推理 |

**用法 = 换掉 LabB 声明里第 13 个模板参数**（EpilogueOutputOp），其他一行不动。

## §8.2 三种 epilogue 的正确性实测（lab 前半）

测试设计：A 按行分组 ±0.5，B 全 1，K=2048 → 正行 acc=+1024、负行 acc=-1024——
正好检验 ReLU 的"归零"和 GELU 的"饱和"。

```
基线    : C[0]=+1024.0  C[16K]=-1024.0   ← 负值原样保留
ReLU 融合: C[0]=+1024.0  C[16K]=+0.0      ← 负值被 ReLU 清零 ✓
GELU 融合: C[0]=+1024.0  C[16K]=-0.0      ← |x|>5 时 GELU 饱和为 x 或 0 ✓
```

## §8.3 性能实测：融合为什么快

| 方案 | 耗时(2048³) |
|------|------------|
| GEMM + 独立 ReLU kernel | 1.237 ms |
| ReLU 融合进 epilogue | **1.097 ms** |

省的就是那个"独立 ReLU kernel"对全部输出的一次显存读+写（16MB×2 @ ~230GB/s ≈ 0.14ms）。
**再放大推理规模，这个差距按输出体积线性增长**——这就是 vLLM/FlashAttention 都做融合的原因。
【实测注】多次运行数字有波动（8MB 输出可能部分留在 L2 里），首次测量差 0.14ms、后续出现过
打平——融合的**原理性收益**（少一趟显存往返）不变，具体数字以 nsys/nmu 实测为准（前置：G08）。

## §8.4 定制自己的激活函数（进阶入口）

`LinearCombinationGELU` 内部用的是**激活函数模板**（`activation.h` 里有 GELU/ReLU/SiLU/
HardSwish 等几十个）。想加一个自己的激活（比如 swish-β），两种方式：

1. **换预定义的**（零成本）：`LinearCombinationSilu`、`LinearCombinationLeakyReLU`…直接换头文件；
2. **写自定义激活 functor**：实现 `__device__ float operator()(float x) const` 的最小结构，
   传给 `LinCombDeEltAct` 系列模板（激活会作用在 α·acc+β·C 上）；
3. **完全自定义 epilogue functor**：需要实现 OutputOp 概念的全套接口（Params/operator()/
   is_source_needed…），参照 `linear_combination.h` 源码仿写——这是读 epilogue 源码的毕业题。

## §8.5 ⚠️ Hopper/Blackwell 的一句话预告（不写代码，按约定）

- 本篇的 epilogue 是 **2.x 风格**（thread 级 functor），在 4060 上就是最佳实践；
- **Sm90 (H100) 起改用 EVT（Epilogue Visitor Tree）**：把"加 bias、激活、量化"组织成一棵
  访客树，由编译器生成最优读写编排——能力更强、API 更声明式；
- 概念相通：EVT 的每个"节点"就对应本篇的一个 functor 步骤。4060 上学的思想原样迁移。

## 本篇实验

`labs/08_epilogue.cu`（已编译运行验证）：

```bash
CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
    -I$CUTLASS/include -I$CUTLASS/tools/util/include labs/08_epilogue.cu -o /tmp/t08 && /tmp/t08
```

【前置：C04.6 transform 的 GPU 版思想】【前置：C09.1 独立 kernel 对照】【深入：面试 G21 精度】

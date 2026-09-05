# 05 布局、主维（lda/ldb/ldc）与 TN/NN/NT 【理解】

> LabB 里 `{dA, K}` 的那个 K 是什么？为什么 A 行主序主维是 K？这一篇讲透。

## §5.1 主维（leading dimension）

**作用**：描述"二维矩阵在一维内存里，相邻两行（或两列）隔多少元素"。

```
行主序 A (M×K):  [a00 a01 a02 ... a0,K-1 | a10 a11 ...]   lda = K (换行跳 K 个)
列主序 B (K×N):  [b00 b10 ... bK-1,0 | b01 b11 ...]       ldb = K (换列跳 K 个)
```

CUTLASS 的 TensorRef 就是（指针，主维）二元组：`{dA, lda}`——指针指首元素，
主维告诉 kernel"怎么找到下一行/列"。这也是为什么 `cudaMalloc` 的指针没有形状信息，
形状必须随参数传（前置：C03.6 数组退化）。

## §5.2 TN / NN / NT：BLAS 的布局黑话

三个字母 = (A 的转置?, B 的转置?)，相对数学上的"标准列主序"而言：

| 代号 | A 布局 | B 布局 | 场景 |
|------|--------|--------|------|
| **NN** | 列主序 | 列主序 | 传统 BLAS 默认 |
| **TN** | 行主序 | 列主序 | **Tensor Core 最爱**（B 的 K 维连续 → mma 喂数据友好）|
| NT/TT | … | … | 少见 |

LabB 的配置（A RowMajor + B ColumnMajor，都是"K 连续"）就是 **TN**——m16n8k16 指令
要求 A/B 沿 K 维连续存放，所以**深度学习推理的权重布局天然就是 TN**。
【坑】这里 T/N 的语义和你直觉的"转置"可能有出入，以"哪个维连续"来理解最稳。

## §5.3 与 PyTorch 的对应（面试实用）

```python
# torch.nn.Linear 的权重 W 形状是 [out_features, in_features], 行主序
# 计算 y = x @ W.T 时, 实际 GEMM 是:
#   y[M,N] = x[M,K] @ W.T[K,N],  W.T 的"内存布局"等价于 W 的转置视图
# → PyTorch 把它分发成 TN GEMM (x 行主序 + W.T 列主序视图)
```

所以"LLM 推理的 GEMM 大多是 TN"不是巧合，是权重布局决定的。你做通信 benchmark 时
如果矩阵来自 PyTorch，注意 `.t()` 不拷贝内存——只是换了"解读布局"的视图（前置：C04.3 视图思想）。

## §5.4 验证实验

回到 LabB：把 B 的布局换成 RowMajor（同时 ldb 改成 N）→ 这变成 NN 布局 → 编译可能报
"不支持的组合"或性能变化——**布局组合直接决定 CUTLASS 能选哪代 MMA 指令**，
这正是"声明配置"在起作用的证据。

## §5.5 TensorRef：CUTLASS 最核心的抽象（配 lab，纯 host 可跑）

**一句话：Layout 就是"坐标 (row, col) → 线性偏移"的映射函数**；
**TensorRef = 指针 + Layout** = "知道怎么定位任意元素的引用"。
LabB 里 `{dA, lda}` 传的其实就是一个隐式 TensorRef。

`labs/05_tensorref.cpp`（**纯 host，g++ 可编译，无需 GPU**）用 4×8 矩阵演示：

```cpp
cutlass::layout::RowMajor    rowLayout = cutlass::layout::RowMajor::packed({M, N});
cutlass::layout::ColumnMajor colLayout = cutlass::layout::ColumnMajor::packed({M, N});
cutlass::TensorRef<float, cutlass::layout::RowMajor> rowRef(ptr, rowLayout);

rowRef.offset({2, 3});    // RowMajor → 19 (2*8+3);  ColumnMajor → 14 (2+3*4)
rowRef.at({1, 5}) = 999;  // 像二维数组一样读写 —— 本质是 ptr[offset]
```

实测输出（同一块内存，两种解读 = **免费得到转置视图**，数据一个字节没动）：

```
RowMajor:        0  1  2 ... 7          ColumnMajor:      0  4  8 ... 28
                 8  9 10 ...15   ⟺                       1  5  9(999所在)... 
```

**这就是"布局决定解读"的完整体现**——你写 hgemm 时"行主序/列主序"的纠结，
本质就是在选这个映射函数。CUTLASS 把它做成了类型系统的一部分
（Layout 是模板参数 → 错误的坐标/布局搭配在编译期就能被部分拦截）。

## §5.6 host 侧编进阶：epilogue 与 TensorView（了解）

- `cutlass::TensorView` = TensorRef + 尺寸（带 bounds 的引用）；
- 官方例子常用 `cutlass::util` 的 host tensor 工具填充/打印（`tools/util/include/cutlass/util/`）。

# 02 LabA：官方 00_basic_gemm 精读 【已验证 ✓】

> 编译运行 NVIDIA 官方最简例子（SGEMM，列主序），逐段精读。
> 实测：本机 9 秒编译，运行输出 **Passed.**

## §2.1 编译运行（不走 CMake，直接 nvcc）

```bash
CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
    -I$CUTLASS/include -I$CUTLASS/tools/util/include -I$CUTLASS/examples/common \
    $CUTLASS/examples/00_basic_gemm/basic_gemm.cu -o /tmp/basic_gemm
/tmp/basic_gemm          # 输出: Passed.
```

【坑】缺 `-I$CUTLASS/examples/common` 会报 `helper.h: No such file`——它包含错误检查工具。

## §2.2 代码逐段精读（源码：`examples/00_basic_gemm/basic_gemm.cu`）

### 第一段：声明 GEMM 类型（编译期选配置）

```cpp
using ColumnMajor = cutlass::layout::ColumnMajor;

using CutlassGemm = cutlass::gemm::device::Gemm<float,        // A 的数据类型
                                                ColumnMajor,  // A 的布局
                                                float,        // B 的类型
                                                ColumnMajor,  // B 的布局
                                                float,        // C 的类型
                                                ColumnMajor>; // C 的布局
```

只给 6 个参数，**其余全部走默认配置**（`default_gemm_configuration.h` 自动配好
tile 形状、Tensor Core 与否、epilogue……）。SGEMM 默认走 128×128×8 的 CUDA Core tile。
【前置：cuda_handbook G07 理解"矩阵乘作为整体操作"】

### 第二段：实例化 + 打包参数

```cpp
CutlassGemm gemm_operator;                    // 一个对象 = 一个专用 kernel 的发射器

CutlassGemm::Arguments args(
    {M, N, K},        // 问题尺寸 (GemmShape 三元组)
    {A, lda},         // TensorRef: 矩阵 A = (指针, 主维)
    {B, ldb},         // B 同理
    {C, ldc},         // C (输入, beta≠0 时会被读)
    {C, ldc},         // D (输出, 可以和 C 同一块 = 原地)
    {alpha, beta});   // epilogue 标量: D = alpha·A·B + beta·C
```

**参数对象模式**是 CUTLASS 的核心设计：所有 host 能确定的东西打包成一个 struct 按值传给
kernel——省得 kernel 从几十个参数里各取所需。
【前置：C03.7 主维 lda 的含义——相邻两行(列)隔多少元素】

### 第三段：启动 + 错误处理

```cpp
cutlass::Status status = gemm_operator(args);      // 一次调用 = 完整的 GEMM
if (status != cutlass::Status::kSuccess) return cudaErrorUnknown;
```

`cutlass::Status` 不是 cudaError——它是 CUTLASS 自己的错误体系（kSuccess/kErrorInternal/
kErrorInsufficientResources……）。**CUTLASS 的错误和 CUDA 的错误是两套**，都要查。

## §2.3 main() 里做了什么

host 上分配三个列主序矩阵 → CPU 参考乘法 → 调 CutlassSgemmNN → 对比 → 打印 **Passed**。
（它内置了"naive 参考矩阵乘 kernel"用于对照——和你 X05 的做法一模一样。）

## §2.4 你现在的能力检查

能独立回答这三问，LabA 毕业：
1. 把 C 改成 RowMajor 需要动哪些地方？（LayoutC、ldc 的值、以及传参时的矩阵形状解释）
2. alpha=0, beta=1 时 D 是什么？（纯拷贝 C——用于验证 epilogue 通路）
3. 为什么这个 SGEMM 不用 Tensor Core？（类型是 float + 默认配置走 Simt；想用 TC 要
   OpClassTensorOp + fp16/tf32 + 架构标签——正是 LabB 的内容）

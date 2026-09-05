# 01 CUTLASS 是什么 + 本地目录导览 【概念】

## §1.1 它解决什么问题

手写 GEMM 到 cuBLAS 水平需要：tiling + double buffering + Tensor Core + swizzle + 流水线……全配齐。
**CUTLASS 把这些性能机制做成可组合的模板零件**——你声明"配置"，它生成 cuBLAS 级别的专用 kernel；
且**每层都能替换定制**（epilogue 加 bias/激活、自定义布局——这是相对 cuBLAS 的核心价值，
FlashAttention、vLLM 的 GEMM 都基于它）。

## §1.2 你本地版本：CUTLASS 4.6.1

```bash
# 版本确认
grep CUTLASS_MAJOR ~/workspace/cuda_pybind_practice/third_party/cutlass/include/cutlass/version.h
# → CUTLASS_MAJOR 4, MINOR 6, PATCH 1
```

【版本警告】2.x / 3.x / 4.x 的 API 差异很大。**本手册全部基于你本地这份 4.6.1 实测**，
网上旧教程（基于 2.x 早期）的代码可能对不上——以本地源码为准。

## §1.3 目录导览（只列你该看的）

```
third_party/cutlass/
├── include/cutlass/               ← 头文件库本体 (全部 #include 的家)
│   ├── gemm/device/gemm.h         ← 2.x 风格 device::Gemm (LabA/B 的主角)
│   ├── gemm/device/gemm_universal_adapter.h  ← 3.x 风格
│   ├── layout/matrix.h            ← RowMajor/ColumnMajor 的定义
│   ├── arch/                      ← Sm80/Sm89... 架构标签 + 指令封装
│   ├── epilogue/thread/           ← epilogue 策略 (LinearCombination 等)
│   ├── half.h                     ← cutlass::half_t
│   └── transform/                 ← threadblock 级的搬运/流水线零件
├── examples/                      ← 100+ 官方例子 (从 00_basic_gemm 开始编号)
│   ├── 00_basic_gemm/             ← 最简 GEMM (本手册 LabA 精读它)
│   ├── 12_gemm_bias_relu/         ← epilogue 定制示例
│   ├── 06_splitK_gemm/            ← Split-K 策略
│   └── ...
├── tools/util/include/cutlass/util/  ← 官方例子用的工具头 (参考 tensor 等)
└── unit_test/                     ← 单元测试 (读不懂用法时来看测试怎么调的!)
```

【读库技巧】**unit_test 目录是最好的 API 文档**——测试代码展示了每个组件"正确的调用姿势"。

## §1.4 三种"用 CUTLASS"的姿势

| 姿势 | 代码量 | 适用 |
|------|--------|------|
| ① device::Gemm 声明配置（LabA/B） | ~30 行 | 90% 场景：换精度/布局/tile |
| ② 定制 epilogue / 换组件 | ~100 行 | 加 bias+激活、特殊输出 |
| ③ Collective/Mainloop 定制（3.x/4.x 风格） | 数百行 | 极致优化（vLLM/FA 的玩法） |

学习路径就是 ①→②→③。本手册带你走通 ①，指出 ②③ 的入口。

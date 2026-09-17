# L06 Linalg 与算子融合：JD 核心词的落地（重点章）

**【前置标注】** L02（dialect 概念）、L05（声明式思想）。GPU 知识：CUTLASS 三级 Tile（cutlass_handbook/02）、epilogue 融合（cutlass_handbook/08）——本章大量类比。

**【记忆钩子】** Linalg = **声明式算子库**："我要 matmul，怎么循环编译器你看着办"。一行 `linalg.matmul` 能 lower 成朴素循环、能 tile 成块循环、能 lower 成 GPU kernel——**同一个描述，多条下降路线，这正是 JD 里"MLIR+Linalg 框架"的意思**。

**【人话版】** 你用 CUTLASS 时写过 `GemmShape<128,128,32>`——那是**手动**选 tile。Linalg 的世界：算法只写"我是 matmul"（一行），tile 尺寸是编译器命令行参数（`--linalg-tile="tile-sizes=2,2,2"`），循环结构是 lowering 自动生成的。**你手动做的，它自动做；你要学的是它怎么自动做。**

---

## 一、名字 → 作用详解

| 名字 | 是什么 |
|---|---|
| `linalg` dialect | 线性代数声明式算子集：matmul/conv/Pooling/泛型 generic |
| 声明式（declarative） | 只说"做什么"（输入输出 + 迭代空间 + 计算公式），不说"怎么做"（循环顺序/分块/并行） |
| `linalg.matmul ins(...) outs(...)` | ins=输入，outs=输出（同时是累加器）。**注意没有循环！** |
| `linalg.generic` | 万能模板：用 affine_map 描述索引映射，可以自定义任何元素级算子 |
| lowering（下降） | 高层 op 变低层 op 的过程：linalg → scf.for + memref（CPU 方向）或 → gpu/NVVM（GPU 方向） |
| bufferize | tensor（值语义，函数式）→ memref（内存语义，可就地改）的翻译层。GPU/CPU 上真跑必须 bufferize |
| tiling（分块） | 把大循环切成小块循环（cache 友好 / 配合 tensor core）。**CUTLASS 的 GemmShape 就是手动版** |
| 算子融合（fusion） | 相邻算子共享迭代空间合成一个循环，中间结果留在寄存器不出内存 |

## 二、实验 1：一行 matmul 变三层循环（lab: `labs/06_linalg_matmul/matmul_memref.mlir`）

```mlir
func.func @matmul(%A: memref<4x4xf32>, %B: memref<4x4xf32>, %C: memref<4x4xf32>) {
  linalg.matmul ins(%A, %B: memref<4x4xf32>, memref<4x4xf32>) outs(%C: memref<4x4xf32>)
  return
}
```

```bash
mlir-opt-15 --convert-linalg-to-loops matmul_memref.mlir
```

**本机实测输出**（完整保存在 `matmul_loops.mlir`）：

```mlir
scf.for %arg3 = %c0 to %c4 step %c1 {
  scf.for %arg4 = %c0 to %c4 step %c1 {
    scf.for %arg5 = %c0 to %c4 step %c1 {
      %0 = memref.load %arg0[%arg3, %arg5] : memref<4x4xf32>   // A[i][k]
      %1 = memref.load %arg1[%arg5, %arg4] : memref<4x4xf32>   // B[k][j]
      %2 = memref.load %arg2[%arg3, %arg4] : memref<4x4xf32>   // C[i][j]
      %3 = arith.mulf %0, %1 : f32
      %4 = arith.addf %2, %3 : f32
      memref.store %4, %arg2[%arg3, %arg4] : memref<4x4xf32>
    }
  }
}
```

对照 L01 的 `matmul.c`——**编译器从一行声明生成了你手写的三重循环**。这就是"声明式 → 命令式"的 lowering。

## 三、实验 2：自动 tiling —— CUTLASS GemmShape 的自动版（最重磅）

```bash
mlir-opt-15 --linalg-tile="tile-sizes=2,2,2" matmul_memref.mlir
```

**本机实测输出**（保存在 `matmul_tiled.mlir`）：

```mlir
scf.for %i   = %c0 to %c4 step %c2 {        // M 方向步长2
  scf.for %j = %c0 to %c4 step %c2 {        // N 方向步长2
    scf.for %k = %c0 to %c4 step %c2 {      // K 方向步长2
      %0 = memref.subview %A[%i, %k] [2, 2] : memref<4x4xf32> to memref<2x2xf32>
      %1 = memref.subview %B[%k, %j] [2, 2] : ...
      %2 = memref.subview %C[%i, %j] [2, 2] : ...
      linalg.matmul ins(%0, %1) outs(%2)     // 仍然是 linalg.matmul！但作用在 2x2 块上
    }
  }
}
```

**看懂这个你就懂了 Linalg 的杀手锏**：

| 概念 | Linalg 自动版 | 你手动写的 CUTLASS 版 |
|---|---|---|
| tile 大小 | `tile-sizes=2,2,2` 命令行参数 | `GemmShape<128,128,32>` 模板参数 |
| 切块访问 | `memref.subview`（零拷贝视图） | `TensorRef` + 坐标偏移（cutlass 05） |
| tile 后仍是 matmul | 是（递归下降的中间态） | 你写的主循环结构 |
| 换 tile 大小 | 改个数字重跑 | 改模板参数重新编译 |

**tile 完之后内部那个 `linalg.matmul` 还能继续降**——先 tile 再 `--convert-linalg-to-loops` 串起来跑（试试！），或走 `--convert-linalg-to-*.yaml` 进 vector/GPU 世界。**多级下降、每级一个 Pass，这就是 MLIR 的 Multi-Level 本意。**

自动调优（JD 的"自动调优"）在干嘛？——**搜索 tile-sizes 的最优值**。你 4060 上 128x128x32 可能最优，别的卡别的数。Triton 的 `@triton.autotune` 本质就是把这组数在运行时扫一遍取最快（你 LeetCUDA 里用过）。

## 四、实验 3：tensor 路线与 bufferize（AI 框架的真实入口）

PyTorch/JAX 图导出后先用 **tensor**（值语义：每个 op 产出新张量，不就地改）：

```mlir
// matmul_tensor.mlir
func.func @matmul(%A: tensor<4x4xf32>, %B: tensor<4x4xf32>, %C: tensor<4x4xf32>) -> tensor<4x4xf32> {
  %0 = linalg.matmul ins(%A, %B: ...) outs(%C: ...) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}
```

真机执行前必须转成内存语义（memref），这一步叫 **bufferize**：

```bash
mlir-opt-15 --one-shot-bufferize --convert-linalg-to-loops matmul_tensor.mlir
```

实测输出里能看到三件值得认识的东西（完整在 `matmul_tensor_loops.mlir`）：

```mlir
%0 = bufferization.to_memref %arg1 : ...      // tensor → memref 的桥
%3 = memref.alloc() {alignment = 128 : i64}   // 为"新张量"开缓冲区（对齐128！）
memref.copy %2, %3 : ...                       // 值语义=要拷贝，编译器自动插入
```

**alignment=128 是什么**？——就是你 CUDA 手册 G04 里 pinned memory / 全局内存访问的对齐知识：128B 对齐让访存按 cache line 整块走。编译器插个 alloc 都讲究这个。**你的底层知识全在给编译器的输出做注脚。**

## 五、算子融合：JD"算子融合"在 MLIR 里的形态

### 为什么融合（你已深知的答案）

两个 kernel：matmul 写出 C 到显存 → elementwise（如 ReLU）再读回 C 改写。一趟显存来回是纯浪费。CUTLASS 的解法：**epilogue 融合**（cutlass_handbook/08，`LinearCombinationWithGELU` 一把梭在 epilogue 里）。

### MLIR 的解法：结构化循环 + 共享迭代空间

MLIR 里 matmul 和 elementwise 都是"结构化"op（都带迭代空间描述）。两个**迭代空间兼容**的相邻算子（比如 `4x4` 的 linalg.matmul 之后跟 `4x4` 的 linalg.generic 求ReLU），编译器可以合并成**一个循环**：中间张量整个留在寄存器/L1，不落地。这就是"producer-consumer fusion"。

**眼见为实版（手工模拟融合结果）**：融合前两段循环、中间量过内存；融合后一段循环、`%acc` 直接接 ReLU 计算。你 epilogue 篇干的事在 CPU/IR 世界重演：

```
融合前：for: C[i][j] = A·B … 写C
        for: D[i][j] = relu(C[i][j]) … 读C 写D     ← C 过一趟内存
融合后：for: D[i][j] = relu(A·B 直接接上)           ← C 从未存在
```

### 上游现状（诚实版，面试别吹过头）

MLIR 上游把 fusion 做在 **linalg-on-tensors 的 TileAndFuse + transform dialect** 体系里（`--test-transform-dialect-...` 系列实验 pass；生产级流水线在 IREE/XLA 里各自实现）。一条命令直观演示的 pass 在 15 里没有——**这章不给你跑假实验**。但概念链你已经全部掌握：epilogue（手动融合）→ 迭代空间共享（融合的可行性判据）→ 中间量不落地（融合的收益）。面试讲这条链，比背"MLIR 支持算子融合"这句话高两个段位。

## 六、踩坑实录

### ⚠️ 坑1：tensor 直接 `--convert-linalg-to-loops` 直接崩溃（段错误）

MLIR 15 的该 pass 只吃 memref。**tensor 必须先 `--one-shot-bufferize`**。（这是上游已知问题，16+ 的 bufferization 才逐步统一。崩溃 backtrace 亲测于此，勿怕，不是你环境坏了。）

### ⚠️ 坑2：`--linalg-tile` 的 tile-sizes 顺序

`tile-sizes=2,2,2` 依次对应 linalg op 迭代空间维度（matmul 是 M,N,K 即 i,j,k 三个循环）。写错顺序（如 2,4）不报错但切出来不均——结果仍正确（subview 循环会处理边界），性能稀碎。**和 CUTLASS GemmShape<M,N,K> 的顺序语义对齐着记。**

### ⚠️ 坑3：`#map = affine_map<...>` 突然出现

subview 之后 memref 类型里多出的 `#map` 是**布局映射**（线性索引 = d0*stride0 + d1*stride1 + offset），就是 cutlass 05 里 Layout 的"坐标→偏移"函数。见到别慌，是老朋友。

## 七、记忆钩子 + 人话版总结

- **钩子**："Linalg 声明式，ins/outs 无循环；tile 一条命令切块，GemmShape 自动版；tensor 要先 bufferize，融合=迭代空间共享。"
- **人话版**：你手动优化的三板斧（分块、融合、对齐），Linalg 全都声明式化了。编译器的活就是把你脑子里的优化流程写成 Pass 流水线。**你已经知道终点，现在你知道了路线。**

## 八、自测题

1. `--linalg-tile="tile-sizes=2,2,2"` 后，为什么循环里**还是** `linalg.matmul`？（tiling 只是包裹，把大问题切成子问题；子问题仍是 matmul，递归下降——多级 IR 的精髓）
2. `memref.subview` 为什么是零拷贝？（只是带 offset/stride 的视图，如同 `int* p = &A[i][k]`——不搬数据，改"怎么算地址"）
3. 值语义的 tensor 为什么必然引入 `memref.copy`？（函数式不可变 → 每个新值要有自己的家；优化 copy 是 bufferization 研究的核心，"bufferization 还有二次消 copy 的 pass"——去查 `bufferization-...` 系列练查文档）
4. 把 tile-sizes 改成 `4,4,4`，输出循环还剩几层？先预测再实验。（3 层循环各只跑 1 次；subview 变 4x4=整块——退化为不分块）
5. GELU epilogue（cutlass 08）若用 MLIR 做，对应什么形态？（matmul 的 linalg op 后接 elementwise linalg.generic，迭代空间共享做 TileAndFuse）

## 九、下一步

- L07：把编译栈全景拼上——Triton 的 TTIR/TTGIR 到 PTX 全链路、XLA/TVM/IREE 各家站位、你的国产 GPU 课题和这一切的关系、面试话术总装。

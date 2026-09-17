# L03 mlir-opt 实操：官方 Pass 初体验与浮点陷阱

**【前置标注】** L02（会读 .mlir 文件）。无需写代码，本章全程只用 `mlir-opt-15` 一个命令。

**【记忆钩子】** mlir-opt 之于 MLIR = ncu 之于 CUDA（不写代码也能做实验的瑞士军刀）。canonicalize 消 `+0`，整数消浮点不消——**编译器尊重 IEEE 754 到让你意外**。

**【人话版】** 前两章你认识了 IR 长什么样。这章把官方现成的优化 Pass 当玩具拆：看它怎么改 IR、什么时候"该优化却不优化"。最后那个浮点陷阱是面试黄金题——99% 的人第一次都答错。

---

## 一、名字 → 作用详解

| 命令 | 作用 |
|---|---|
| `mlir-opt-15 输入.mlir --某pass` | 解析 → 跑指定 Pass → 打印结果 |
| `--canonicalize` | 规范化：代数化简（x+0→x、x*1→x、常量折叠…） |
| `--cse` | 公共子表达式消除（Common Subexpression Elimination） |
| `--mlir-print-ir-after-all` | 打印每个 Pass 跑完后的 IR（看流水线神器） |
| `--pass-pipeline='builtin.module(cse,canonicalize)'` | 把多个 Pass 串成流水线（嵌套括号指定作用于哪层） |

## 二、实验 1：canonicalize 与著名的浮点陷阱（lab: `labs/03_canonicalize/`）

文件 `canonicalize_int.mlir`：

```mlir
func.func @test(%x: i32) -> i32 {
  %0 = arith.constant 0 : i32
  %1 = arith.addi %x, %x : i32
  %2 = arith.addi %1, %0 : i32     // x + x + 0
  %3 = arith.muli %1, %1 : i32
  %4 = arith.addi %2, %3 : i32
  return %4 : i32
}
```

```bash
mlir-opt-15 --canonicalize canonicalize_int.mlir
```

**本机实测输出**（`+0` 被消掉，`(x+x)` 和 `(x+x)*(x+x)` 里的公共项被 CSE 式共享）：

```mlir
func.func @test(%arg0: i32) -> i32 {
  %0 = arith.addi %arg0, %arg0 : i32
  %1 = arith.muli %0, %0 : i32
  %2 = arith.addi %0, %1 : i32
  return %2 : i32
}
```

文件 `canonicalize_float.mlir`（**把 i32 全换成 f32 再跑**）：

```mlir
func.func @float_trap(%x: f32) -> f32 {
  %zero = arith.constant 0.0 : f32
  %one = arith.addf %x, %x : f32
  %two = arith.addf %one, %zero : f32   // 想当然：x+x+0.0
  return %two : f32
}
```

**本机实测输出：`+ 0.0` 原封不动留着！**

### 为什么？——IEEE 754 的两个暗礁（面试黄金题）

1. **负零**：`(-0.0) + 0.0 = +0.0`，但 `-0.0 ≠ +0.0`（比较相等时相同，但 copysign/bitcast 会区分）。消掉 `+0.0` 会改变结果。
2. **NaN**：`NaN + 0.0 = NaN` 看似没变，但 NaN 的 payload 位和"异常标志"可能被清除。

所以**整数世界里恒等的 x+0=x，在浮点世界不恒等**。MLIR 的 canonicalize 按最保守的语义来，宁可不优化。想让它优化？在 op 上加 fastmath 属性（`arith.fastmath<fast>`）显式声明"我放弃这些边角语义"——CUDA 里 `--use_fast_math`、Triton 里 `tl.dot(..., allow_tf32)` 是同一个思想：**精确性和速度的合同由程序员签字**。

> 呼应你的经验：CUTLASS 里 fp32 accumulate 的 mma、fp16 输入——数值语义每一层都讲究，编译器也一样。

## 三、实验 2：CSE 公共子表达式消除

文件 `cse_demo.mlir`：

```mlir
func.func @cse_demo(%a: f32, %b: f32) -> f32 {
  %x = arith.mulf %a, %b : f32
  %y = arith.mulf %a, %b : f32    // 和 %x 完全一样
  %r = arith.addf %x, %y : f32
  return %r : f32
}
```

```bash
mlir-opt-15 --cse cse_demo.mlir
```

实测：两个 `mulf` 只剩一个，第二个引用直接指 `%x`。（grep 验证：mulf 计数 = 1）

**人话版**：CSE 就是"同样的菜别炒两遍"。在 GPU 语境下这就是**算子融合要解决的冗余计算**的一种——两个 kernel 各算一遍同样的中间量，融合后算一次传两次。

## 四、实验 3：流水线观察（看 Pass 串行工作）

```bash
mlir-opt-15 --cse --canonicalize --mlir-print-ir-after-all cse_demo.mlir 2>&1 | head -60
```

会看到 IR 以 `// *** IR Dump After CSE ***`、`// *** IR Dump After Canonicalizer ***` 分段打印——每个 Pass 干了什么一目了然。**这是调试自己 Pass 的第一手段**（L04/L05 写的工具有同款选项）。

## 五、踩坑实录

### ⚠️ 坑：`--pass-pipeline` 括号写法报错

流水线语法是 `--pass-pipeline='builtin.module(func.func(cse,canonicalize))'`——**要指定每层作用域**（module 层 vs 函数层）。新手直接 `--pass-pipeline='cse'` 会报 `unknown pass pipeline`。用 `--cse --canonicalize` 平铺写法（mlir-opt 自动放对层）最省心，初学阶段全用它。

### ⚠️ 坑：浮点实验做不出"陷阱效果"

如果你跑 float 版 `+0` 也被消了——检查是不是不小心给 op 加了 fastmath 属性，或者用了别人魔改过的 mlir-opt。官方 15.0.7 行为如本文：**不消**。

## 六、记忆钩子 + 人话版总结

- **钩子**："整数 +0 是零，浮点 +0 是爹（惹不起）。canonicalize 化简、CSE 去重、dump-after-all 看流水线。"
- **人话版**：mlir-opt 是不用写一行 C++ 就能玩的实验台。玩到本章结束，你对"Pass 就是改 IR 的函数"已经有手感了——下一章亲手写一个。

## 七、自测题

1. `arith.addf %x, %x` 能被 canonicalize 改成 `mulf %x, 2.0` 吗？先预测再实验。（提示：x+x 精确等于 x*2，无舍入差异——实测 15.0.7 的 canonicalize 不做这个，需要自己写规则，这正是 L05 的实验！）
2. `--mlir-print-ir-after-all` 和 `--mlir-print-op-generic` 区别是什么？（前者按 Pass 分段看演化，后者脱语法糖看结构）
3. fastmath 属性是放弃了什么换性能？（IEEE 754 的结合律/负零/NaN 语义保证）
4. CSE 在 CUDA 里对应的"手工版"是什么？（把重复计算的中间量存 register/共享内存复用；跨 kernel 的重复中间量→融合）

## 八、下一步

- L04：把 mlir-opt 拆开——用 200 行 C++ 写一个属于你自己的 mlir-opt。

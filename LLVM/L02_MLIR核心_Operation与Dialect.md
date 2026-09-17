# L02 MLIR 核心：一切皆 Operation，一族皆 Dialect

**【前置标注】** L01（知道 LLVM IR 长什么样）。C++ 的继承/多态（cpp_handbook/C02）读代码时用得上。

**【记忆钩子】** LLVM IR 是"一种语言定死"，MLIR 是"一套造语言的乐高"。积木只有一种形状——Operation；主题包叫 Dialect；搭出来的程序是一棵 Operation 树。

**【人话版】** LLVM IR 很好，但它离"机器"太近、离"算法"太远——AI 框架想要的是"这是一个 matmul"这种高层信息，LLVM IR 里只剩循环和 load/store，信息全丢了。MLIR 的答案：允许**多层 IR 共存**，高层（linalg.matmul）逐层下降（loops → vector → llvm），每层只做自己该做的优化。**多层级**（Multi-Level）就是名字的由来。

---

## 一、名字 → 作用详解

### 1. Operation：MLIR 宇宙的唯一积木

MLIR 程序里**所有东西都是 Operation**：函数是 op、常量是 op、循环是 op、连最外层的 module 都是 op。一个 Operation 由六部分组成（先记前四个）：

```
%1 = arith.addf %a, %b : f32
└┬┘ └────┬─────┘ └─┬─┘ └┬┘ └─┬─┘
结果    操作名+操作数  类型
                （Operation 通用格式：结果 = 方言.操作(操作数) : 类型）
```

| 部件 | 含义 | 类比 |
|---|---|---|
| 结果 `%1` | 这个 op 产出的值（SSA，见 L01） | 表达式的左值 |
| 名字 `arith.addf` | 谁家的什么操作：**方言.操作** | 命名空间.函数名 |
| 操作数 `%a, %b` | 输入值 | 函数实参 |
| 类型 `f32` | 类型标注 | 函数签名 |
| Region | op 身上挂的"代码块容器"（循环体/函数体都是） | 嵌套语句块 |
| 属性 | 编译期常量信息 `{value = 0 : i32}` | C++ 的 constexpr 参数 |

### 2. 一切皆 Operation 的证据：泛型打印

MLIR 的"漂亮格式"是语法糖。`--mlir-print-op-generic` 让一切现出原形（**本机实测输出**）：

```
"builtin.module"() ({            ← module 是 op
  "func.func"() ({               ← 函数是 op
  ^bb0(%arg0: i32):              ← bb = BasicBlock，块
    %0 = "arith.constant"() {value = 0 : i32} : () -> i32
    %1 = "arith.addi"(%arg0, %arg0) : (i32, i32) -> i32
    "func.return"(%4) : (i32) -> ()
  }) {function_type = (i32) -> i32, sym_name = "test"} : () -> ()
}) : () -> ()
```

所有 op 都是同一个形状：`"名字"(操作数) (区域) {属性} : 类型`。**这就是"一切皆 Operation"**——MLIR 只需要一套机制（遍历/改写/验证）就能处理所有 op，这是它设计的聪明之处。

### 3. 树形结构：Operation → Region → Block → Operation

```
builtin.module                ← 根（Operation）
└── Region                   ← module 挂一个区域
    └── Block ^bb0           ← 区域里是块（基本块：线性执行，只有出口跳转）
        ├── func.func        ← 块里又是 Operation……套娃
        │   └── Region
        │       └── Block ^bb0(%arg0)
        │           ├── arith.addi
        │           └── func.return
```

记住这个包含关系，L04 你写 `walk` 遍历时看到的后序输出（先子后父）就是这棵树的深度优先遍历。

### 4. Dialect：方言 = 一族 Operation 的"姓氏"

| 方言 | 管什么 | 重要性 |
|---|---|---|
| `builtin` | module 等根级 op | ★ |
| `func` | 函数定义/调用 | ★★ |
| `arith` | 算术（整数/浮点加减乘） | ★★★ 高频出没 |
| `scf` | 结构化控制流（for/if/while） | ★★★ |
| `memref` / `tensor` | 内存块 / 值语义张量 | ★★★（L06 主角） |
| `linalg` | 线性代数声明式算子（matmul/conv） | ★★★★ **JD 点名** |
| `vector` | SIMD 向量 op（通向 GPU 也在用） | ★★★ |
| `llvm` | LLVM IR 的 MLIR 化身（最后一站） | ★★ |
| `gpu` / `nvgpu` / `nvvm` | GPU 相关 | ★★（串你的背景） |

查本机注册了哪些方言：`mlir-opt-15 --show-dialects`（能列出 60+ 个）。

**人话版**：dialect 就是个"主题包"。func 包管函数、arith 包管加减乘除、linalg 包管矩阵乘。每家编译器（Triton/IREE/XLA）都能定义自己的包，塞进同一个 MLIR 框架里互通——**这就是各家人挤进来做 AI 编译器的公共底座**。

### 5. MLIR vs LLVM IR 一张表

| | LLVM IR | MLIR |
|---|---|---|
| 层级 | 单层（已经很底层） | 多层（高层算法 → 低层机器） |
| 积木 | 指令（add/load/br） | Operation（什么都能当） |
| 扩展 | 加指令要动 LLVM 核心 | **定义新 dialect 零侵入** |
| 典型起点 | C/C++ 前端 | Python/Torch/自定义 DSL |
| 典型终点 | x86/ARM 汇编 | 降到 llvm dialect → 复用 LLVM 后端 |

**关系不是替代，是上下游**：MLIR 一路降到最后变成 `llvm.func` 等低层 op，转成真 LLVM IR，交给 LLVM 后端出机器码。MLIR 是"漏斗的上半段"。

## 二、样例：读一段 .mlir（lab: `labs/02_operation_tree/smoke_test.mlir`）

```mlir
// 一个 MLIR 文件的完整样子。注释是我的：
func.func @test(%x: f32) -> f32 {     // func 方言定义函数，@test 是符号名
  %0 = arith.constant 0.0 : f32       // 产生一个常量 op，结果叫 %0
  %1 = arith.addf %x, %x : f32        // %1 = x + x
  %2 = arith.addf %1, %0 : f32        // %2 = %1 + 0.0
  return %2 : f32                     // 返回（func 方言的终止 op）
}
```

自己动手（三条命令，30 秒）：

```bash
cd labs/02_operation_tree
mlir-opt-15 smoke_test.mlir                     # 原样吐回（没指定 pass = 只解析验证）
mlir-opt-15 --mlir-print-op-generic smoke_test.mlir   # 脱掉语法糖看真身
mlir-opt-15 --show-dialects                     # 本机认识的全部方言
```

第三条能跑通还有一层意义：**解析器认识 `arith.addf` 是因为解析时注册了方言**。L04 你写自己的工具时也要 `registerAllDialects(registry)`，否则解析直接报错——到时会再遇到它。

## 三、踩坑实录

### ⚠️ 坑：`error: unknown dialect`

写了 `foo.bar` 这种不存在的方言，或者工具没注册该方言。解法：检查拼写；用 `--show-dialects` 查；实验文件顶部可加 `// RUN:` 之外的一行说明自己依赖哪些方言。

### ⚠️ 坑：漂亮格式 vs 泛型格式混着看容易懵

`%0 = arith.addi ...` 和 `"arith.addi"(...) : ...` 是**同一个 op 的两种打印**。网上教程两种混用，别当成两种语法。

## 四、记忆钩子 + 人话版总结

- **钩子**："一切皆 Operation，一族皆 Dialect，一层 Region 一层 Block，多层下降通 LLVM。"
- **人话版**：MLIR = 乐高。积木只有一种（Operation），主题包随便换（Dialect），拼出来的模型从"矩阵乘"一路拆碎成"机器指令"（多层下降），最后一段路交给 LLVM 这个老司机。

## 五、自测题

1. `"func.return"(%4) : (i32) -> ()` 用漂亮格式写出来是什么？
2. `%1` 为什么不能被赋值两次？（SSA——L01 学的概念在 MLIR 原样适用）
3. `arith.addf` 和 `linalg.matmul` 都是"算东西的 op"，抽象层级差在哪？（元素级 vs 张量级/声明式）
4. 为什么 Triton 要定义自己的 `triton` 方言而不是直接用 arith？（需要表达 block/tile 级并行语义，现有方言表达不了——自定义 dialect 零成本正是 MLIR 的卖点）

## 六、下一步

- L03：用 `mlir-opt` 跑真正的优化 Pass，见识著名的浮点陷阱。

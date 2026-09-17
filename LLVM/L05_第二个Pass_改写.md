# L05 第二个 Pass：改写（RewritePattern 模式）

**【前置标注】** L04（Pass 骨架三件套）。C++：继承与虚函数（C02）。

**【记忆钩子】** 看 Pass 用 walk + errs()，**改 Pass 用 Pattern + Rewriter**。一条改写规则 = 一个 `matchAndRewrite`；一堆规则交给 Greedy 引擎反复扫到不动点。

**【人话版】** 上章的 Pass 只会"看"。真正的优化器要"改"——但直接手改 IR 树会改出悬空引用、破坏 SSA，像不系安全带上高架。MLIR 给的安全带叫 **PatternRewriter**：你声明"见到什么换成什么"（matchAndRewrite），引擎保证换完之后整棵树仍然合法。**JD 里"算子融合/自动调优"的代码在底层几乎全是这套 Pattern 机制。**

---

## 一、名字 → 作用详解

| 名字 | 作用 |
|---|---|
| `OpRewritePattern<OpT>` | 一条改写规则的基类：只对 `OpT` 这种 op 感兴趣 |
| `matchAndRewrite(op, rewriter)` | 规则本体。返回 `failure()` = 不匹配别动；`success()` = 我改完了 |
| `PatternRewriter &rewriter` | 唯一被允许的"改 IR 的手"。创建新 op、替换、擦除都通过它（它会同步维护父子关系/引用） |
| `replaceOpWithNewOp<NewOp>(旧op, 参数...)` | 一步完成"创建新 op + 把旧 op 的所有使用者改指向新 op + 删除旧 op" |
| `RewritePatternSet` | 规则的集合（一个 Pass 可以装几十条规则） |
| `applyPatternsAndFoldGreedily` | 贪婪引擎：反复在 IR 上跑所有规则直到一条都匹配不上（不动点），顺带常量折叠 |

### Pattern 思想为什么吊打手改

你只需要回答一个问题："**什么样的 op，换成什么样的 op**"。找遍全树、保持合法、迭代到收敛——脏活全归引擎。这就是"声明式"的好处：**和 Linalg 声明式算子是同一个哲学**（L06 见）。

## 二、实验代码（lab: `labs/05_rewrite_pass/`）

`MulToAdd.cpp` 核心 60 行（完整注释版在 lab）：

```cpp
// 规则：addf %x, %x  →  mulf %x, 2.0
struct AddFSelfToMulF : public OpRewritePattern<arith::AddFOp> {
  using OpRewritePattern<arith::AddFOp>::OpRewritePattern;  // 继承构造

  LogicalResult matchAndRewrite(arith::AddFOp op,
                                PatternRewriter &rewriter) const override {
    Value lhs = op.getLhs();
    Value rhs = op.getRhs();
    if (lhs != rhs)
      return failure();                    // x+y 不是 x+x，不管

    auto two = rewriter.create<arith::ConstantFloatOp>(
        op.getLoc(), APFloat(2.0f), rewriter.getF32Type());
    rewriter.replaceOpWithNewOp<arith::MulFOp>(op, lhs, two);
    return success();
  }
};

// Pass 本体：收集规则，交给贪婪引擎
struct MulToAddPass
    : public PassWrapper<MulToAddPass, OperationPass<func::FuncOp>> {
  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<AddFSelfToMulF>(&getContext());
    (void)applyPatternsAndFoldGreedily(getOperation(), std::move(patterns));
  }
};
// 注册 + main 与 L04 相同（选项名换成 --mul-to-add）
```

### 运行结果（本机实测）

输入 `test.mlir`：

```mlir
func.func @demo(%x: f32, %y: f32) -> f32 {
  %a = arith.addf %x, %x : f32    // x+x  →  应改写
  %b = arith.addf %x, %y : f32    // x+y  →  不应改写
  %c = arith.addf %b, %b : f32    // b+b  →  应改写（连锁反应）
  return %c : f32
}
```

```bash
./my-opt-rw ../test.mlir --mul-to-add
```

输出：

```mlir
func.func @demo(%arg0: f32, %arg1: f32) -> f32 {
  %cst = arith.constant 2.000000e+00 : f32
  %0 = arith.addf %arg0, %arg1 : f32     // x+y：failure() 放行，原样保留
  %1 = arith.mulf %0, %cst : f32         // b+b 变 b*2（注意 %0 是新 addf，连锁生效）
  return %1 : f32
}
```

**三个看点**：
1. `x+x` 消失，被 `mulf %x, 2.0` 取代；
2. `x+y` 一字未动（failure() 的语义）；
3. `b+b` 里的 b 是第一条规则**改完之后产生的新值**——贪婪引擎迭代到不动点，规则对改写结果再次生效。这就是"引擎帮你循环"的意思。

### 为什么这个改写是安全的（对照 L03 陷阱）

浮点里 `x+x` 与 `x*2.0` **逐位相等**（都是精确翻倍，尾数左移一位，不触发舍入差异；连 NaN/Inf 行为都一致）。对比 L03 的 `x+0.0 ≠ x`（负零/NaN 坑）——**同样是"代数恒等式"，过不过 IEEE 754 这一关是天壤之别**。面试被问"什么能优化什么不能"时，这两个例子一起举，直接封神。

（诚实备注：MLIR 官方 canonicalize 没内置这条规则，不是因为它不安全，是收益太小没进默认集——所以正好留给我们当教材。）

## 三、构建命令（与 L04 同构）

```bash
cd labs/05_rewrite_pass && mkdir build && cd build
cmake .. -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm \
         -DMLIR_DIR=/usr/lib/llvm-15/lib/cmake/mlir -DCMAKE_BUILD_TYPE=Release
make -j8 && ./my-opt-rw ../test.mlir --mul-to-add
```

CMakeLists 相比 L04 多链了 `mlir/Transforms/GreedyPatternRewriteDriver.h` 所在的库（lab 里已写好，实际都在 MLIRPass/MLIRTransforms 里）。

## 四、踩坑实录

### ⚠️ 坑1：头文件路径 MLIR 15 是 `Arithmetic` 不是 `Arith`

`#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"`（15 的目录名）。**16+ 改名成 `Arith`**——网上新教程的 `#include "mlir/Dialect/Arith/IR/Arith.h"` 在 15 上编译不过，反过来的旧教程在新版上也不过。看到 include 报错先查版本目录名：`ls /usr/lib/llvm-15/include/mlir/Dialect/`。

### ⚠️ 坑2：matchAndRewrite 里用了 op 的结果再 replace

`replaceOpWithNewOp` 之后 `op` 就被删除了，之后再 `op->getLhs()` 是悬垂访问（UB，可能崩溃）。**先取好需要的 Value，再替换**——lab 代码里 `lhs/rhs` 提前存局部变量就是为了这个。

### ⚠️ 坑3：规则没生效？检查 Value 相等比较

`lhs != rhs` 比较的是**同一个 SSA 值**（同一个 `%x`），不是"值恰好相等"。`addf %x, %y` 即使运行时 x==y 也不改——编译期我们根本不知道运行时的值。**编译器优化只吃"语法上可判定"的等价**。

## 五、和已知知识挂钩

| Rewrite 概念 | 你已会的对应物 |
|---|---|
| Pattern + 引擎到不动点 | 自动调优：定义搜索空间，框架迭代到收敛 |
| failure() 放行 | kernel launch 前检查前置条件不满足就 return |
| replaceOpWithNewOp 原子替换 | shared_ptr 的"先建新的、再换指针、旧的自动析构"（RAII 思想） |
| 一堆 Pattern 组成 Pass | 一堆 CUTLASS epilogue functor 组成融合策略 |

## 六、记忆钩子 + 人话版总结

- **钩子**："一条规则一个类，match 管 gate、rewrite 管换，failure 放行 success 换完，Greedy 引擎扫到不动点。"
- **人话版**：改 IR 别裸奔，系 PatternRewriter 安全带。你只管说"见 x+x 换 x*2"，找位置、改引用、循环到收敛全是引擎的事。

## 七、自测题

1. 亲手加一条规则：`mulf %x, 1.0 → %x`（浮点乘 1，注意！这**不**恒等——想想为什么；改成整数版 `muli %x, 1 → %x` 是安全的，动手实现整数版）。
2. `matchAndRewrite` 标了 `const`，怎么还能"修改" IR？（修改全走 rewriter 参数，this 不变——const 正确性和"改"不冲突，设计精髓）
3. 把 `applyPatternsAndFoldGreedily` 的返回值 `(void)` 去掉改成检查 `failed(...)` 并 `signalPassFailure()`——什么时候会失败？（有 op 被标 illegal 或 folding 出错；本实验不会触发，但生产 Pass 必查）
4. 预测：给 test.mlir 里加一行 `%d = arith.addf %c, %c : f32`，输出会怎样？（`mulf %1, 2.0`——连锁继续，%c 本身已是 mulf）

## 八、下一步

- L06：正式进入 JD 核心词 **Linalg**——声明式算子、matmul 的自动 lowering、算子融合在 MLIR 里长什么样。

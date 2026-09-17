# L04 第一个 Pass：抄写（PrintOpNames 完整解析）

**【前置标注】** L02（Operation 树）、L03（mlir-opt 用法）。C++：虚函数/CRTP（cpp_handbook C02）、unique_ptr（C02.6）、lambda（C02.7）。CMake：最小工程 + find_package（cmake_handbook K01/K03）。

**【记忆钩子】** Pass = 遍历 Operation 树的函数对象。三件套：**继承 PassWrapper、实现 runOnOperation、注册成命令行选项**。抄一遍，MLIR 就"入门"了——这是知乎高赞回答方法论的第一步。

**【人话版】** 这章把 L03 玩的 mlir-opt 换成自己写的 my-opt。代码只有一个 cpp 文件 80 行，但它能跑 `--canonicalize` 等全部官方 Pass，还多一个我们自制的 `--print-op-names`。编译岗笔试/面试聊到"你写过 MLIR Pass 吗"，这 80 行就是你的底气。

---

## 一、代码总览（lab: `labs/04_my_first_pass/`）

`PrintOpNames.cpp` 三段式结构（完整代码带注释在 lab 里，此处讲解每一段）：

```
① Pass 本体      struct PrintOpNamesPass : PassWrapper<自己, OperationPass<ModuleOp>>
                     └─ 重写 runOnOperation()
② 注册           static PassPipelineRegistration<>("print-op-names", "描述", 回调)
③ 工具主体       main() { registerAllDialects; MlirOptMain(...) }
```

### ① Pass 本体：三行核心

```cpp
struct PrintOpNamesPass
    : public PassWrapper<PrintOpNamesPass, OperationPass<ModuleOp>> {
  void runOnOperation() override {
    getOperation()->walk([](Operation *op) {
      llvm::errs() << "访问到 op: " << op->getName() << "\n";
    });
  }
};
```

逐行拆：

| 代码 | 解释 |
|---|---|
| `PassWrapper<自己, OperationPass<ModuleOp>>` | CRTP（cpp_handbook C02 讲过：基类拿派生类当模板参数，编译期生成样板代码）。`OperationPass<ModuleOp>` 表示"这个 Pass 以 ModuleOp 为单位被调度" |
| `runOnOperation()` | Pass 的 main 函数。MLIR 对每个 ModuleOp 调一次 |
| `getOperation()` | 拿到当前正在处理的 Operation（这里是 ModuleOp） |
| `walk(lambda)` | 深度优先遍历子树，每个 Operation 调一次 lambda |
| `op->getName()` | 拿 `"arith.addf"` 这种名字 |
| 没调 `signalPassFailure()` | = Pass 成功 |

**walk 的遍历顺序**（本机实测，值得盯一眼）：

```
访问到 op: arith.mulf      ← 先子
访问到 op: arith.addf
访问到 op: func.return
访问到 op: func.func       ← 后父
访问到 op: builtin.module  ← 最后根
```

**默认是后序遍历**（先孩子后自己）。为什么这么设计？——处理孩子之前父节点还没定型，自底向上做归约类分析最自然（reduce ladder 也是从叶子往上归约，一个道理）。

### ② 注册：让 Pass 变成命令行选项

```cpp
static PassPipelineRegistration<> printOpNamesPipeline(
    "print-op-names", "打印每个 Operation 的名字",
    [](OpPassManager &pm) { pm.addPass(CreatePrintOpNamesPass()); });
```

这是一个**全局变量的构造函数副作用**：main 执行前，它往全局注册表登记"有个叫 print-op-names 的流水线"。之后命令行 `--print-op-names` 就能用了。

### ③ 工具主体：站在官方肩膀上

```cpp
int main(int argc, char **argv) {
  DialectRegistry registry;
  registerAllDialects(registry);   // 不注册方言，解析器不认识 func/arith！
  return asMainReturnCode(
      MlirOptMain(argc, argv, "我的第一个 mlir-opt 工具\n", registry));
}
```

`MlirOptMain` 就是官方 mlir-opt 的 main（解析命令行/读文件/建 context/跑 Pass/打印结果全套）。链上它，我们的 my-opt **天生拥有全部官方 Pass** + 我们注册的私有 Pass。

## 二、构建与运行（已验证）

```bash
cd labs/04_my_first_pass
mkdir build && cd build
cmake .. -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm \
         -DMLIR_DIR=/usr/lib/llvm-15/lib/cmake/mlir \
         -DCMAKE_BUILD_TYPE=Release
make -j8
./my-opt ../test.mlir --print-op-names        # 自己的 Pass！
./my-opt ../test.mlir --print-op-names --canonicalize   # 和官方 Pass 串联！
```

实测输出（`--print-op-names`）：

```
访问到 op: arith.mulf
访问到 op: arith.addf
访问到 op: func.return
访问到 op: func.func
访问到 op: builtin.module
module { func.func @gemm_like(...) ... }      ← 处理后的 IR 照常打印
```

## 三、踩坑实录（三个，全部亲测，按出场顺序）

### ⚠️ 坑1：CMake 报 `check_source_compiles: C: needs to be enabled`

**原因**：`project(xxx CXX)` 只声明了 C++，但 LLVMConfig.cmake 内部要编译 C 探测程序（检测 FFI/Terminfo 库）。

**解法**：`project(my_first_pass C CXX)` —— 写两个语言。

### ⚠️ 坑2：`PassPipelineRegistration` 回调签名编译不过

**现象**：老教程抄来的 `[](PassManager &pm)` 报 no matching function。

**原因**：MLIR 15 里 `PassPipelineRegistration<>`（EmptyPipelineOptions 特化版）要求回调参数是 **`OpPassManager&`**（不是 `PassManager&`！）。两者是父类/子类关系，lambda 参数写子类、调用方传父类，std::function 不认这种逆变，报错信息还相当费解。

**解法**：`[](OpPassManager &pm) { pm.addPass(...); }`

### ⚠️ 坑3：main 返回值类型不匹配

`MlirOptMain` 返回 `LogicalResult`（MLIR 的成功/失败三态哨兵），不能直接 `return`。用配套的 `asMainReturnCode(...)` 包一层转成进程退出码。

## 四、和已知知识挂钩

| MLIR 概念 | 你已会的对应物 |
|---|---|
| walk 遍历 IR 树 | grid-stride loop 遍历数据 |
| Pass 流水线 | CUDA stream 串 kernel：产出接消费 |
| 注册表（全局构造副作用） | `__attribute__((constructor))` / 静态对象初始化（cpp_handbook C02 static 四用法之"全局对象"） |
| signalPassFailure() | kernel 里 `if(出错) { 记录; return; }` |

## 五、记忆钩子 + 人话版总结

- **钩子**："继承 Wrapper、重写 runOnOperation、Registration 挂号、MlirOptMain 白嫖官方全家桶。"
- **人话版**：写 Pass 像考驾照——车（MlirOptMain）是现成的，你只需要证明会开（runOnOperation）并挂上牌（Registration）。三个坑：CMake 要 C+CXX、回调要 OpPassManager&、main 要 asMainReturnCode。

## 六、自测题

1. 把 Pass 模板参数从 `OperationPass<ModuleOp>` 换成 `OperationPass<func::FuncOp>`，会发生什么？（调度粒度变成每个函数一次；test.mlir 只有一个函数所以输出相同，但"访问到 op: func.func"会消失——因为 FuncOp 是调度单位本身不再是 walk 到的子节点。动手试！）
2. 为什么 `printOpNamesPipeline` 要加 `static`？（全局生命周期，main 前构造完成注册；局部变量没跑 main 就析构了——cpp C02 static 四用法）
3. walk 的默认顺序是什么序？怎么改成先序？（后序；`walk<WalkOrder::PreOrder>(...)`——去 llvm-project 源码 `mlir/include/mlir/IR/Visitors.h` 里找，练"把源码当字典查"）
4. 我的 my-opt 和官方 mlir-opt 是什么关系？（同一个 MlirOptMain，多注册了一个私有 Pass——所以官方所有选项它都有）

## 七、下一步

- L05：从"看"升级到"改"——RewritePattern 改写式 Pass，编译器岗位笔试最爱考的形态。

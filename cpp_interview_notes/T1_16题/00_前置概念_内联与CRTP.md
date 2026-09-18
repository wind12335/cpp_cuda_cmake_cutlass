# 前置概念扫盲：内联（inlining）与 CRTP

> 两个被 01 题（虚函数性能）反复引用的概念。读一遍，忘了随时回来。

## 一、函数内联（inlining）

**定义：编译器把函数体"复制粘贴"到调用它的地方，"这次调用"本身消失。**

```cpp
inline int square(int x) { return x * x; }
int y = square(5);   // 编译器改写成 int y = 25; 没有调用发生
```

### ⚠️ 和"内联汇编"是两回事（MMA 学习者的经典混淆）

| 名字 | 含义 | 你在哪见过 |
|------|------|-----------|
| 内联汇编 (inline asm) | 在 C++/CUDA 里手写汇编指令 | `asm volatile("mma.sync.aligned.m16n8k16...")` —— 学 MMA 写的 PTX |
| 函数内联 (inlining) | 编译器优化：函数体复制进调用点 | `__forceinline__ __device__` 修饰的辅助函数 |

MMA 教程里两者总挨着出现（`__forceinline__` 的包装函数包着 `asm` 的 PTX），但一个是"写汇编"，一个是"编译器优化"。

### 为什么它是"优化之母"

省掉跳转/传参是小头（几个 cycle）。大头：**函数体进了调用点，编译器的视野打通了**——
常量传播、死代码消除、循环向量化全部跨过原函数边界生效。

```cpp
for (int i = 0; i < n; ++i) s += square(i);
// 内联后编译器看到: s += i*i;  → 可向量化/变形
// 不内联: 每圈一次黑盒调用, 什么都不好说
```

不内联的函数 = 一堵优化墙。**虚函数的最大代价就是"不可内联"竖起了这堵墙**（详见 01_exp_虚调用成本.cpp 实测：0.91 vs 0.18 ns）。

### 关键字只是建议

- `inline`：链接语义（允许多编译单元重复定义）+ 内联建议，编译器可拒绝（第 26 题）；
- `__forceinline__`（MSVC）/`__attribute__((always_inline))`（GCC）才近乎强制；
- CUDA 的 `__forceinline__ __device__`：把小函数焊进 kernel，避免 device 端真函数调用的
  寄存器/**ABI** 开销（ABI = Application Binary Interface，函数调用的二进制约定——参数放哪些寄存器、
  返回值放哪、哪些寄存器调用者负责保存。真函数调用必须遵守这套约定，内联之后连约定都不用遵守了）。

## 二、CRTP（奇特递归模板模式）

**定义：编译期多态——把"调用哪个函数"从运行期查虚表，挪到编译期写死+内联。**

```cpp
template <class D>                    // D = 将来的派生类
struct AnimalBase {
    void speak() { static_cast<D*>(this)->speak_impl(); }
    // this 是 AnimalBase<D>*，强转成 D*；D 编译期已知 → 直接焊死调用，无查表
};

struct Dog : AnimalBase<Dog> {        // "奇特递归": Dog 继承"以 Dog 为参数"的基类
    void speak_impl() { printf("Woof\n"); }
};
struct Cat : AnimalBase<Cat> {
    void speak_impl() { printf("Meow\n"); }
};
Dog d; d.speak();   // 编译期确定调 Dog::speak_impl 且已内联 —— 机器码 ≈ 手写直调
```

### virtual vs CRTP 对比

| | virtual | CRTP |
|---|---------|------|
| 分发时机 | 运行期（vptr→虚表→间接跳转） | 编译期（写死+内联） |
| 性能 | 单态 ~0.9ns / 多态 ~6ns（实测） | ~0.18ns，与直调打平 |
| 运行期混装类型 | ✅ `vector<Animal*>` 混装 Dog/Cat | ❌ 类型编译期定死 |
| 代码体积 | 一份实现共用 | 每个实例化各一份（膨胀） |
| 典型用户 | PyTorch dispatcher | CUB/Thrust functor、Eigen |

### 选择标准：先问两个问题，再看场景清单

**判定问题①：这个类型，编译的时候知道吗？** 不知道（运行期才定）→ 只能 virtual。
**判定问题②：分发的粒度多细？** 每个数据元素一次（尤其 GPU）→ 必须模板；每个任务/请求/图节点一次 → virtual 无所谓。

#### virtual 的场景（运行期才知道类型 + 分发不细）

1. **算子图执行引擎**（推理框架内核）：计算图从模型文件加载，GEMM/ATTN/COMM 节点
   **混装**在 `vector<unique_ptr<Op>>` 里逐个 `run()`——节点类型加载图时才知道，
   每节点分发一次。你课题里的"领域大模型计算图"就是这个形态。实测混装能力：

   ```cpp
   std::vector<std::unique_ptr<Op>> pipeline;   // 一个容器混三种不同算子
   pipeline.push_back(std::make_unique<ScaleV>());
   pipeline.push_back(std::make_unique<AddV>());   // ← CRTP/模板做不到: 类型不同放不进同一容器
   for (auto& op : pipeline) x = op->apply(x);     // 运行期逐个分发 ✓
   ```
2. **PyTorch dispatcher**：`aten::mm` 被调用的那一刻才知道 device/dtype → 运行期挑注册实现。每个 op 分发一次。
3. GUI/游戏实体、插件系统、测试 mock——事件驱动，人手速级频率，性能无关紧要。

#### 模板/CRTP 的场景（编译期已知 + 每元素粒度）

1. **CUDA kernel 里的 functor**（CUB/Thrust/CUTLASS epilogue）：functor 类型编译期定死 →
   `operator()` 内联进 kernel。**若用 virtual：每个 thread 每个元素一次虚分发 = 一次
   vptr 显存间接读（几百 cycle）+ GPU 没有乱序执行帮你藏 → kernel 直接报废**（X03 主角）。
2. **CUTLASS `GemmShape<128,128,32>`**：tile 常数编进类型 → 循环完全展开、寄存器静态分配。
3. `std::sort` 传 functor vs C 的 `qsort` 传函数指针：functor 内联进排序内循环（函数指针不可内联）。
4. Eigen 表达式模板：`A*B+C` 编译期融合成单循环、零临时矩阵。
5. 你写 mma 时的 `__forceinline__ + template<int M,N,K>` 包装。

#### 工业界标配：混血架构（外虚内模）

PyTorch 全栈就是这个套路：**外层** dispatcher 用 virtual/运行期注册做"每个 op 一次"的
粗分发；**内层**选中的 kernel 全是模板，吃掉"每个元素一次"的细粒度。
一天分发几百次的事用 virtual，一秒分发几十亿次的事用模板——**频率 × 单位成本**决定生死。

#### 实测插曲：CPU 上 virtual 到底多贵？（诚实版）

在 CPU 上做"1 亿次热循环 virtual vs 模板"对照，两次都**没拉开差距**（实测）：
第一次被**浮点依赖链**掩盖（乱序执行把虚调用开销藏进浮点延迟）；
第二次被 **GCC 去虚化**直接消除（编译器看穿了真实类型，虚调用变直调）。
结论恰好说明：**CPU 上偶尔虚一次根本不疼，硬件和编译器到处帮你藏**。
真正的生死线在两处：**GPU**（无乱序执行 + vptr 在显存，每元素虚分发=灾难）和
**优化墙**（虚调用不可内联 → 编译器看不见函数体 → 向量化/常量传播全断，
这才是 01_exp 量出 0.91 vs 0.18ns 的机制根源）。

实测对照见 `01_exp_虚调用成本.cpp`；虚调用机制本体见 `01_虚函数与多态.md`。

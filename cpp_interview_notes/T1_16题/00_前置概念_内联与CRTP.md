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

### 选择标准

需要"运行期才决定类型、容器混装" → **virtual**（每层/op 分发一次，成本可摊薄）；
类型编译期已知、热路径零开销 → **CRTP/模板**（CUDA 里 functor 进 kernel 必须走这条路，
否则逐元素虚分发 = 每元素一次显存间接读，kernel 直接报废）。

实测对照见 `01_exp_虚调用成本.cpp`；虚调用机制本体见 `01_虚函数与多态.md`。

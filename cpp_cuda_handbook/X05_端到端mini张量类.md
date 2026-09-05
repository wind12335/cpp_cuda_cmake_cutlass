# X05 端到端毕业项目：一个 mini 张量类 【毕业】

> 把 C01~C09 + G02~G06 + X01~X04 全部用上：写一个**安全、可移动、能算**的 GPU 张量类。
> 写完它，你就具备了读 LeetCUDA 工程代码的全部前置知识。

## §X05.1 设计目标

```cpp
Tensor t1(1024);            // 分配 1024 个 float 的显存
t1.fill(1.0f);              // kernel 填充
Tensor t2 = t1.scale(2.0f); // 新张量 = t1 × 2 (模板 functor, 前置 X03)
Tensor t3 = t1 + t2;        // 运算符重载 + 泛型 kernel (前置 C05.5/X03.5)
float  s   = t3.sum();      // 规约 kernel (前置 G05)
std::vector<float> host = t3.toHost();   // 搬回主机 (前置 G04)
```

## §X05.2 类骨架与知识点映射

```cpp
class Tensor {
public:
    explicit Tensor(size_t n);                    // X01 RAII: 构造时 cudaMalloc
    Tensor(const Tensor&) = delete;               // X01 禁拷贝
    Tensor(Tensor&& o) noexcept;                  // C07 移动(容器可存)
    ~Tensor();                                    // X01 自动 cudaFree

    Tensor scale(float k) const;                  // X03 functor: Scale
    Tensor operator+(const Tensor& o) const;      // C05.5 运算符重载 + X03.5 zipWith
    float  sum() const;                           // G05 规约 + atomic(简化版)
    std::vector<float> toHost() const;            // G04 cudaMemcpy D2H
    void fill(float v);                           // X03 常数 functor
    size_t size() const;  float* get() const;
private:
    float* d_ = nullptr;                          // C03 指针
    size_t n_ = 0;
};
```

## §X05.3 实现要点（lab 里有完整代码）

1. **构造**：cudaMalloc + 打印（教学用）；析构：cudaFree（RAII，X01）；
2. **禁拷贝 + 可移动**：vector<Tensor> 可用、双重释放不可能（X01）；
3. **scale/add 返回新张量**：函数内构造结果张量并 `return`——**依赖 RVO/移动**，零多余拷贝
   （前置 C07.4：千万别 return std::move）；
4. **sum**：简化版用 atomicAdd 单点累加（教学）；工程版用两段式规约（前置 G05）；
5. **所有 kernel 启动走 X02 的 launch 包装器**：错误必查。

## §X05.4 扩展练习（自测）

1. 加 `Tensor relu() const`（X03 的 ReLU functor）；
2. 加 `static Tensor fromHost(const std::vector<float>&)` 工厂函数；
3. 把填充/规约换成你 G05 写的 shuffle 版规约；
4. 加一个 `cudaStream_t` 成员，让每个张量的操作走自己的流（X04 思想）。

## 本篇实验

`labs/X05_minitensor.cu`——完整可编译的 Tensor 类 + main 里的端到端演示（fill→scale→add→sum→toHost）。

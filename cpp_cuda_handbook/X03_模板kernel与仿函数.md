# X03 模板 Kernel 与仿函数 【进阶】

> 用 30 行代码写出"你自己的 Thrust"：一个能套任意元素级运算的泛型 kernel。
> 【前置：C08 模板与 functor】【CUDA G03 合并访存】

## §X03.1 目标形态

**作用**：让 GPU 上的元素级变换像标准库一样好用。

```cpp
DeviceBuffer d(1024);
transformGPU(d.get(), 1024, Scale(2.0f));          // 仿函数版
transformGPU(d.get(), 1024, []__device__(float x){ return x + 1; });  // lambda 版
```

## §X03.2 关键点：functor 必须在 device 端可调用

**作用**：lambda 里的 `__device__` 标记（CUDA 11.0+ 支持扩展 lambda）或仿函数的 `__device__` 方法。

```cpp
struct Scale {
    float k;
    explicit Scale(float k) : k(k) {}
    __device__ float operator()(float x) const { return x * k; }   // ← device 端可调用
};
```

【前置：C08.6】functor 比函数指针强的原因：类型是编译期已知的 → `operator()` **内联进
kernel**——这就是 CUB/Thrust 零开销抽象的全部秘密。

## §X03.3 泛型 transform kernel

**作用**：元素级运算的通用模板（对照 std::transform，前置 C04.6）。

```cpp
template <class F>
__global__ void transformKernel(const float* in, float* out, int n, F f) {
    int stride = gridDim.x * blockDim.x;
    for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += stride)
        out[i] = f(in[i]);           // f 内联在这里 —— 零虚调用开销
}

template <class F>
void transformGPU(float* d, int n, F f) {
    int block = 256;
    int grid = (n + block - 1) / block;
    transformKernel<<<grid, block>>>(d, d, n, f);   // 原地
    cudaDeviceSynchronize();
}
// F 是模板参数 → 每个 functor 生成专门版本的 kernel → operator() 完全内联
```

## §X03.4 为什么不用函数指针或 virtual？

| 方案 | 开销 | 原因 |
|------|------|------|
| 模板 functor | 0（内联） | 类型编译期已知 |
| 函数指针 | 一次间接跳转，且 device 端指针参数传递麻烦 | 目标运行期确定 |
| virtual | 查表 + 不可内联 + 显存里放 vtable | 目标运行期确定 |

（实测数据见 T1-01 虚调用实验；device 端每次间接读都是显存级延迟。）

## §X03.5 二元运算：把"向量加法"也泛化

```cpp
template <class F>
__global__ void zipWith(const float* a, const float* b, float* c, int n, F f) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = f(a[i], b[i]);
}
// zipWith<<<...>>>(da, db, dc, n, []__device__(float x, float y){ return x + y; });
```

【心智模型】transform / zipWith / reduce 三个泛型算子 + functor = 函数式 GPU 编程的最小集
（Thrust 的 transform/reduce 就是这么搭的）。

## 本篇实验

`labs/X03_functor.cu`——一元 transform（Scale / relu lambda）+ 二元 zipWith（add）+ 正确性验证。

```bash
nvcc -std=c++17 -O2 -arch=sm_89 --extended-lambda labs/X03_functor.cu -o /tmp/x03 && /tmp/x03
```

⚠ **`--extended-lambda` 必须加**：带 `__device__` 标记的 lambda（裸写）需要这个 flag，
否则报 "annotation on lambda requires --extended-lambda"——忘了它你会以为代码写错了。

# X02 Kernel 包装器与错误处理 【核心】

> 目标：把"启动一个 kernel"包装成**不可能忘记检查错误**的函数。

## §X02.1 C++ 异常风格的 CUDA 错误处理

**作用**：替代"每个 API 后面跟一个 if"——抛异常 + 析构自动清理（前置：C06 RAII）。

```cpp
#include <stdexcept>
#include <string>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) \
        throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(e) + \
                                 " at " #x " line " + std::to_string(__LINE__)); \
} while (0)

void f() {
    DeviceBuffer buf(bytes);          // RAII(前置 X01)
    CUDA_CHECK(cudaMalloc(...));      // 出错 → 抛异常 → buf 析构自动释放 → 零泄漏
}
```

## §X02.2 类型安全的 kernel 启动包装

**作用**：把"<<<>>> + 两次错误检查"收进一个函数，调用方只剩业务。

```cpp
template <class Kernel, class... Args>
void launch(Kernel k, dim3 grid, dim3 block, Args&&... args) {
    k<<<grid, block>>>(std::forward<Args>(args)...);   // 完美转发(前置 C07.6)
    cudaError_t e = cudaGetLastError();
    if (e != cudaSuccess)
        throw std::runtime_error(std::string("kernel launch 失败: ") + cudaGetErrorString(e));
}
// 调用:
launch(vecAdd, grid, block, da, db, dc, n);   // 一行, 错误必查, 参数类型由编译器把关
```

【前置：C07.6 完美转发】【坑】launch 失败（配置错误）**当场**可查；执行错误要
`cudaStreamSynchronize`/`cudaDeviceSynchronize` 后才可见——两处都要查。

## §X02.3 launch config 的计算函数

**作用**：把"算 grid 大小"也收编成函数，消灭手写向上取整的重复。

```cpp
dim3 grid1d(int n, int block = 256) {
    return dim3((n + block - 1) / block);
}
// 用法: launch(vecAdd, grid1d(n), dim3(256), da, db, dc, n);
```

## §X02.4 更进一步：带配置对象的启动器

**作用**：把 grid/block/shared/流 打包成一个结构，复杂框架（如 vLLM 的 kernel 启动）都这么组织。

```cpp
struct LaunchConfig {
    dim3 grid, block;
    size_t shmem = 0;
    cudaStream_t stream = nullptr;     // nullptr = 默认流
};

template <class Kernel, class... Args>
void launchConfigured(Kernel k, LaunchConfig c, Args&&... args) {
    k<<<c.grid, c.block, c.shmem, c.stream>>>(std::forward<Args>(args)...);
    CUDA_CHECK(cudaGetLastError());
}
```

## 本篇实验

`labs/X02_wrapper.cu`——异常风格错误处理 + launch 包装器 + LaunchConfig，用向量加法端到端演示。

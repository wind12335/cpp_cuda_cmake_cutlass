# G02 第一个 Kernel：启动语法与错误检查 【基础】

## §G02.1 三种函数修饰符

**作用**：告诉编译器"这个函数在哪边运行、从哪边调用"。

| 修饰符 | 在哪执行 | 谁能调用 | 用途 |
|--------|---------|---------|------|
| `__global__` | device | **host**（也能 device，动态并行不常用） | **kernel 入口**，返回值必须 void |
| `__device__` | device | 只能 device | kernel 的辅助函数 |
| `__host__` | host | host | 普通 C++ 函数（可省略不写） |

可以组合：`__host__ __device__` 表示"两边都能编、两边都能调"（数学工具函数常用，G09 速查）。

## §G02.2 kernel 启动语法

**作用**：`<<<grid, block>>>` 三尖括号是 CUDA 特有语法，指定"开多少线程"。

```cpp
kernel<<<gridDim, blockDim, sharedMemBytes, stream>>>(arg1, arg2);
//          ↑ block 组数 ↑每组线程数  ↑动态共享内存(默认0) ↑流(默认0)
// 例: 总共 1024 个线程的两种等价写法
vecAdd<<<1, 1024>>>(d_a, d_b, d_c, n);      // 1 个 block × 1024 线程
vecAdd<<<8, 128>>>(d_a, d_b, d_c, n);       // 8 个 block × 128 线程 (更常用!)
```

grid/block 可以是三维 `dim3 g(4, 4, 1)`——二维图像/矩阵问题用 `blockIdx.y`。

## §G02.3 错误检查宏（每个程序必备）

**作用**：CUDA 错误是静默的，不检查就等于瞎跑。

```cpp
#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { \
        printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); \
        exit(1); \
    } } while (0)

CUDA_CHECK(cudaMalloc(&d_a, bytes));            // 每个会出错的 API 都包上
kernel<<<grid, block>>>(...);
CUDA_CHECK(cudaGetLastError());                 // launch 后查启动错误
CUDA_CHECK(cudaDeviceSynchronize());            // 同步后查执行错误
```

【前置：面试 G10 排查三板斧】kernel 的异步错误会"滞后"到后面的 API 才冒出来。

## §G02.4 完整最小程序：向量加法（hello world of CUDA）

```cpp
#include <cstdio>
#include <cuda_runtime.h>

__global__ void vecAdd(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;   // 我的全局编号
    if (i < n) c[i] = a[i] + b[i];                   // 越界保护!
}

int main() {
    int n = 1 << 20;                                 // 1M 元素
    size_t bytes = n * sizeof(float);
    float *ha = new float[n], *hb = new float[n], *hc = new float[n];
    for (int i = 0; i < n; ++i) { ha[i] = 1.0f; hb[i] = 2.0f; }

    float *da, *db, *dc;
    cudaMalloc(&da, bytes); cudaMalloc(&db, bytes); cudaMalloc(&dc, bytes);
    cudaMemcpy(da, ha, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(db, hb, bytes, cudaMemcpyHostToDevice);

    int block = 256;
    int grid = (n + block - 1) / block;              // 向上取整: 保证覆盖全部元素
    vecAdd<<<grid, block>>>(da, db, dc, n);

    cudaMemcpy(hc, dc, bytes, cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();                         // 等待完成 + 让错误可见
    printf("c[0]=%f c[%d]=%f\n", hc[0], n-1, hc[n-1]);   // 3.0 3.0

    cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] ha; delete[] hb; delete[] hc;
    return 0;
}
```

【逐行拆解】`(n + block - 1) / block` = **向上取整除法**：n=1000、block=256 → grid=4，
覆盖 1024 线程 ≥ 1000，多出的 24 个线程靠 `if (i < n)` 拦住。
【前置：C03 §7】`cudaMalloc(&da, ...)` 传二级指针的原因。

## §G02.5 设备查询 API

```cpp
int device; cudaGetDevice(&device);
cudaDeviceProp prop; cudaGetDeviceProperties(&prop, device);
printf("SM 数=%d, shared/SM=%zu KB, 全局内存=%zu MB\n",
       prop.multiProcessorCount, prop.sharedMemPerBlockOptin >> 10,
       prop.totalGlobalMem >> 20);
```

## 本篇实验

`labs/G02_hello.cu`——错误检查宏 + 设备查询 + 向量加法（含 CPU 对照验证）。
【前置：C03 指针】看不懂 `&da` / `*p` 先回 C 手册。

# G09 CUDA 常用 API 与内置变量速查表 【随时查】

## Runtime API（host 端）

| API | 作用 | 备注 |
|-----|------|------|
| `cudaGetDeviceCount(&n)` | GPU 数量 | 多卡判断 |
| `cudaGetDeviceProperties(&prop, i)` | 查参数(SM数/显存/…) | G02 lab |
| `cudaSetDevice(i)` | 切换当前 GPU | 多卡必调 |
| `cudaMalloc(&p, bytes)` | 分配显存 | 参数是二级指针 |
| `cudaMallocHost(&p, bytes)` | 分配 host 锁页内存 | 异步搬运的前提 |
| `cudaMallocManaged(&p, bytes)` | 统一内存 | 原型用，性能路径别用 |
| `cudaMemcpy(dst, src, bytes, dir)` | 同步搬运 | 阻塞直到完成 |
| `cudaMemcpyAsync(..., stream)` | 异步搬运 | 必须配 pinned + 流 |
| `cudaMemset(p, 0, bytes)` | 显存清零 | |
| `cudaFree(p)` | 释放显存/锁页用 cudaFreeHost | |
| `cudaDeviceSynchronize()` | 等全部任务完成 | 错误检查点 |
| `cudaGetLastError()` | 取走最近的错误码 | kernel launch 后必调 |
| `cudaGetErrorString(e)` | 错误码 → 文字 | 配合宏打印 |
| `cudaStreamCreate/Destroy` | 创建/销毁流 | G06 |
| `cudaEventCreate/Record/Elapsed/Destroy` | 事件计时 | G06 |
| `cudaStreamWaitEvent(s, e)` | 流 s 等待事件 e | 跨流依赖 |
| `cudaFuncSetAttribute(k, cudaFuncAttributeMaxDynamicSharedMemorySize, n)` | 提高动态 shared 上限 | >48KB 时必须 |
| `cudaOccupancyMaxPotentialBlockSize(...)` | 让运行时建议 block 大小 | |

## Kernel 侧

| 语法 | 作用 |
|------|------|
| `__global__ void k(...)` | kernel 入口（host 可调） |
| `__device__ float f(...)` | device 辅助函数 |
| `__host__ __device__` | 两边都编译 |
| `__shared__ T s[N];` | block 共享内存（静态） |
| `extern __shared__ T s[];` | 动态共享内存（启动第三参数给字节） |
| `__syncthreads()` | block 内屏障（全体必须到达） |
| `__shfl_down_sync(mask, v, d)` | warp 内寄存器交换 |
| `atomicAdd(p, v)` 等 | 原子操作 |
| `__float2half / __half2float` | fp16 转换 |
| `__expf / __fdividef / rsqrtf` | 快速数学近似（-use_fast_math 同族） |
| `clock64()` | GPU 时钟周期计数（测段内耗时） |

## 内置变量（kernel 里直接用）

| 变量 | 含义 |
|------|------|
| `threadIdx.{x,y,z}` | block 内编号 |
| `blockIdx.{x,y,z}` | grid 内编号 |
| `blockDim.{x,y,z}` | 每 block 的线程数 |
| `gridDim.{x,y,z}` | grid 的 block 数 |

## 常用公式

```
一维全局索引:  i = blockIdx.x * blockDim.x + threadIdx.x
向上取整 grid: grid = (n + block - 1) / block
grid-stride:   for (i = 起点; i < n; i += gridDim.x * blockDim.x)
二维展开:      idx = row * width + col
occupancy:     驻留 warp 数 ÷ 48 (每 SM 上限, 4060)
roofline 拐点: 峰值FLOPS ÷ 峰值带宽
```

## 数据类型（精度家族）

| 类型 | 头文件 | 用途 |
|------|--------|------|
| `float` / `double` | 原生 | fp32 / fp64 |
| `half` (`__half`) | `cuda_fp16.h` | fp16 数据（fp32 累加！） |
| `__nv_bfloat16` | `cuda_bf16.h` | bf16 数据 |
| `float2/float4` | 原生 | 向量化搬运（16B 合并访存） |
| `half2` | `cuda_fp16.h` | 半精度向量化 |

## host 端数据类型转换速查

```cpp
__float2half(f) / __half2float(h)          // fp32 ↔ fp16
__float2bfloat16(f) / __bfloat162float(b)  // fp32 ↔ bf16
reinterpret_cast<float4*>(&v)[0]           // 向量化读取(前置 T2/C01 位运算)
```

【前置：本表是 X 系列（C++↔CUDA 结合部）的弹药库】

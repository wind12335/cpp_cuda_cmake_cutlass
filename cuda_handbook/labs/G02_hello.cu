// G02 实验: 第一个 CUDA kernel —— 向量加法 + 错误检查 + 设备查询
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G02_hello.cu -o g02 && ./g02
#include <cstdio>
#include <cuda_runtime.h>

// 【错误检查宏】每个 CUDA API 都包一层; kernel 后面用 GetLastError + Sync
#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { \
        printf("CUDA error %s @line %d: %s\n", #x, __LINE__, cudaGetErrorString(e)); \
        exit(1); \
    } } while (0)

// __global__: 这是一个 kernel, 从 host 启动, 在 GPU 上执行, 返回值必须 void
__global__ void vecAdd(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;   // 我的全局编号 (G03 详解)
    if (i < n) c[i] = a[i] + b[i];                   // 越界保护: grid 向上取整会多出线程
}

int main() {
    // ---- 0. 设备查询: 知己知彼 ----
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    printf("GPU: %s\n", prop.name);
    printf("SM 数=%d, 每 SM 最大线程=%d, shared/SM=%zu KB, 显存=%zu MB\n",
           prop.multiProcessorCount, prop.maxThreadsPerMultiProcessor,
           prop.sharedMemPerMultiprocessor >> 10, prop.totalGlobalMem >> 20);

    // ---- 1. host 端准备数据 ----
    int n = 1 << 20;
    size_t bytes = n * sizeof(float);
    float *ha = new float[n], *hb = new float[n], *hc = new float[n];
    for (int i = 0; i < n; ++i) { ha[i] = 1.0f; hb[i] = 2.0f; }

    // ---- 2. 显存分配 + 数据上传 ----
    float *da, *db, *dc;
    CUDA_CHECK(cudaMalloc(&da, bytes));     // 二级指针: 函数把地址写进 da (前置 C03 §7)
    CUDA_CHECK(cudaMalloc(&db, bytes));
    CUDA_CHECK(cudaMalloc(&dc, bytes));
    CUDA_CHECK(cudaMemcpy(da, ha, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(db, hb, bytes, cudaMemcpyHostToDevice));

    // ---- 3. 启动 kernel ----
    int block = 256;                        // 每 block 256 线程 (32 的倍数, 常用值)
    int grid = (n + block - 1) / block;     // 向上取整: 4194304/256=16384 个 block
    printf("启动: grid=%d, block=%d, 总线程=%d (元素数=%d)\n", grid, block, grid * block, n);
    vecAdd<<<grid, block>>>(da, db, dc, n);
    CUDA_CHECK(cudaGetLastError());         // 查启动错误 (配置错在这里暴露)
    CUDA_CHECK(cudaDeviceSynchronize());    // 等 GPU 干完 + 查执行错误

    // ---- 4. 结果搬回 + 验证 ----
    CUDA_CHECK(cudaMemcpy(hc, dc, bytes, cudaMemcpyDeviceToHost));
    bool ok = true;
    for (int i = 0; i < n; ++i) if (hc[i] != 3.0f) { ok = false; break; }
    printf("验证: %s, c[0]=%.1f c[last]=%.1f\n", ok ? "全部正确 ✓" : "有错 ✗", hc[0], hc[n - 1]);

    // ---- 5. 清理 ----
    cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] ha; delete[] hb; delete[] hc;
    return 0;
}

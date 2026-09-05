// G06 实验: 单流串行 vs 双流流水线 (pinned + event 计时)
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G06_stream.cu -o g06 && ./g06
#include <cstdio>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

__global__ void heavyKernel(float* d, int n) {          // 故意做重活, 让"算"能和"传"咬合
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float x = d[i];
    for (int k = 0; k < 256; ++k) x = x * 1.0001f + 0.3f;
    d[i] = x;
}

int main() {
    const int N = 32 * 1024 * 1024;                     // 32M float = 128MB
    const size_t bytes = (size_t)N * sizeof(float);
    float *h, *d;
    CUDA_CHECK(cudaMallocHost(&h, bytes));              // pinned! 不然 async 是假的 (G04.4)
    CUDA_CHECK(cudaMalloc(&d, bytes));
    for (size_t i = 0; i < N; ++i) h[i] = 1.0f;

    cudaStream_t s1, s2;
    CUDA_CHECK(cudaStreamCreate(&s1)); CUDA_CHECK(cudaStreamCreate(&s2));
    cudaEvent_t t0, t1;
    CUDA_CHECK(cudaEventCreate(&t0)); CUDA_CHECK(cudaEventCreate(&t1));
    int block = 256, grid = (N + block - 1) / block;

    // ---- 单流: 上传 → 全算 → 下载 (严格串行) ----
    CUDA_CHECK(cudaEventRecord(t0));
    CUDA_CHECK(cudaMemcpyAsync(d, h, bytes, cudaMemcpyHostToDevice, s1));
    heavyKernel<<<grid, block, 0, s1>>>(d, N);
    CUDA_CHECK(cudaMemcpyAsync(h, d, bytes, cudaMemcpyDeviceToHost, s1));
    CUDA_CHECK(cudaStreamSynchronize(s1));
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms1; cudaEventElapsedTime(&ms1, t0, t1);
    printf("单流(传-算-传 串行): %.2f ms\n", ms1);

    // ---- 双流: 两半分别走 s1/s2, 传输与计算互相咬合 ----
    CUDA_CHECK(cudaEventRecord(t0));
    CUDA_CHECK(cudaMemcpyAsync(d, h, bytes / 2, cudaMemcpyHostToDevice, s1));
    CUDA_CHECK(cudaMemcpyAsync(d + N / 2, h + N / 2, bytes / 2, cudaMemcpyHostToDevice, s2));
    heavyKernel<<<grid / 2, block, 0, s1>>>(d, N / 2);
    heavyKernel<<<grid / 2, block, 0, s2>>>(d + N / 2, N / 2);
    CUDA_CHECK(cudaMemcpyAsync(h, d, bytes / 2, cudaMemcpyDeviceToHost, s1));
    CUDA_CHECK(cudaMemcpyAsync(h + N / 2, d + N / 2, bytes / 2, cudaMemcpyDeviceToHost, s2));
    CUDA_CHECK(cudaStreamSynchronize(s1)); CUDA_CHECK(cudaStreamSynchronize(s2));
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms2; cudaEventElapsedTime(&ms2, t0, t1);
    printf("双流(拷贝/计算重叠): %.2f ms  ← 提速 %.0f%%\n", ms2, (1 - ms2 / ms1) * 100);

    cudaStreamDestroy(s1); cudaStreamDestroy(s2);
    cudaEventDestroy(t0); cudaEventDestroy(t1);
    cudaFreeHost(h); cudaFree(d);
    return 0;
}

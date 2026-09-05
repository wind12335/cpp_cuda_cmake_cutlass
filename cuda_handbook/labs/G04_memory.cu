// G04 实验: shared memory 转置(消 bank conflict) + pageable vs pinned 搬运实测
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G04_memory.cu -o g04 && ./g04
#include <cstdio>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

#define TILE 32
#define PAD 1      // padding 消 bank conflict 的开关: 0=有冲突 1=无冲突 (改它对比性能!)

// naive 转置: 读合并, 但写散 (相邻线程写相邻列 → 跨步访问)
__global__ void transposeNaive(const float* in, float* out, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i < n && j < n) out[j * n + i] = in[i * n + j];
}

// shared memory 转置: 读写都合并; PAD 用于消 shared 的 bank conflict
__global__ void transposeShared(const float* in, float* out, int n) {
    __shared__ float tile[TILE][TILE + PAD];   // +PAD: 每行多一格, 列访问错开 bank
    int x = blockIdx.x * TILE + threadIdx.x;
    int y = blockIdx.y * TILE + threadIdx.y;
    if (x < n && y < n) tile[threadIdx.y][threadIdx.x] = in[y * n + x];   // 读: 合并
    __syncthreads();                            // 等全 block 装完 (全体必须到达!)
    if (x < n && y < n) out[x * n + y] = tile[threadIdx.x][threadIdx.y];  // 写: 合并
}

int main() {
    const int n = 4096;
    const size_t bytes = (size_t)n * n * sizeof(float);
    float *d_in, *d_out, *h_pin;
    CUDA_CHECK(cudaMalloc(&d_in, bytes));
    CUDA_CHECK(cudaMalloc(&d_out, bytes));
    CUDA_CHECK(cudaMallocHost(&h_pin, bytes));   // pinned host 内存
    for (size_t i = 0; i < n * n; ++i) h_pin[i] = (float)(i % 7);
    CUDA_CHECK(cudaMemcpy(d_in, h_pin, bytes, cudaMemcpyHostToDevice));

    dim3 block(TILE, TILE), grid(n / TILE, n / TILE);
    cudaEvent_t t0, t1;
    CUDA_CHECK(cudaEventCreate(&t0)); CUDA_CHECK(cudaEventCreate(&t1));

    // naive vs shared
    float ms_naive, ms_shared;
    CUDA_CHECK(cudaEventRecord(t0));
    transposeNaive<<<grid, block>>>(d_in, d_out, n);
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    cudaEventElapsedTime(&ms_naive, t0, t1);
    CUDA_CHECK(cudaEventRecord(t0));
    transposeShared<<<grid, block>>>(d_in, d_out, n);
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    cudaEventElapsedTime(&ms_shared, t0, t1);
    printf("转置 naive =%.3f ms, shared+PAD=%d = %.3f ms\n", ms_naive, PAD, ms_shared);

    // pageable vs pinned: H2D 搬运
    float* h_page = new float[bytes / sizeof(float)];   // ⚠ 大小必须和搬运量一致(初次编写时
                                                        //   只给了 n 个元素, 搬 64MB 越界段错误!)
    double b_gb = bytes / 1e9;
    CUDA_CHECK(cudaEventRecord(t0));
    CUDA_CHECK(cudaMemcpy(d_in, h_page, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms_page; cudaEventElapsedTime(&ms_page, t0, t1);
    CUDA_CHECK(cudaEventRecord(t0));
    CUDA_CHECK(cudaMemcpy(d_in, h_pin, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms_pin; cudaEventElapsedTime(&ms_pin, t0, t1);
    printf("H2D pageable = %6.1f GB/s, pinned = %6.1f GB/s\n", b_gb / ms_page * 1000, b_gb / ms_pin * 1000);

    cudaFree(d_in); cudaFree(d_out); cudaFreeHost(h_pin); delete[] h_page;
    return 0;
}

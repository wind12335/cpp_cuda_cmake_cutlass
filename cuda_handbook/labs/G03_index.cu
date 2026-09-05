// G03 实验: 索引公式 / 合并访存(行读vs列读实测) / grid-stride
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G03_index.cu -o g03 && ./g03
#include <cstdio>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

__global__ void showIndex() {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < 8)   // 只让前几个线程打印, 否则刷屏
        printf("block=%d thread=%d → 全局 i=%d\n", blockIdx.x, threadIdx.x, i);
}

// grid-stride: 数据量与线程数解耦
__global__ void vecAddStride(const float* a, const float* b, float* c, int n) {
    int stride = gridDim.x * blockDim.x;
    for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += stride)
        c[i] = a[i] + b[i];
}

// 行读(合并): 相邻线程地址相邻
__global__ void readRows(const float* m, float* out, int rows, int cols) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < rows * cols) out[i] = m[i];               // 一维展开 = 按行连续
}
// 列读(不合并): 相邻线程地址差 cols*4 字节
__global__ void readCols(const float* m, float* out, int rows, int cols) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < rows * cols) {
        int r = i / cols, c = i % cols;
        out[i] = m[c * rows + r];                      // 列主序式跨步访问
    }
}

int main() {
    // ---- 1. 索引可视化 ----
    showIndex<<<2, 8>>>();
    CUDA_CHECK(cudaDeviceSynchronize());

    // ---- 2. 行读 vs 列读带宽实测 ----
    const int R = 4096, C = 4096;
    const size_t bytes = (size_t)R * C * sizeof(float);
    float *d_m, *d_out;
    CUDA_CHECK(cudaMalloc(&d_m, bytes));
    CUDA_CHECK(cudaMalloc(&d_out, bytes));
    CUDA_CHECK(cudaMemset(d_m, 1, bytes));
    int block = 256, grid = ((size_t)R * C + block - 1) / block;
    cudaEvent_t t0, t1;
    CUDA_CHECK(cudaEventCreate(&t0)); CUDA_CHECK(cudaEventCreate(&t1));

    for (int rep = 0; rep < 2; ++rep) {                // 预热
        readRows<<<grid, block>>>(d_m, d_out, R, C);
        readCols<<<grid, block>>>(d_m, d_out, R, C);
    }
    CUDA_CHECK(cudaEventRecord(t0));
    readRows<<<grid, block>>>(d_m, d_out, R, C);
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms_row; cudaEventElapsedTime(&ms_row, t0, t1);
    double gbs_row = bytes / ms_row / 1e6;

    CUDA_CHECK(cudaEventRecord(t0));
    readCols<<<grid, block>>>(d_m, d_out, R, C);
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float ms_col; cudaEventElapsedTime(&ms_col, t0, t1);
    double gbs_col = bytes / ms_col / 1e6;

    printf("行读(合并):  %.3f ms = %6.1f GB/s\n", ms_row, gbs_row);
    printf("列读(不合并): %.3f ms = %6.1f GB/s   ← 差距就是'合并访存'的价值\n", ms_col, gbs_col);

    // ---- 3. grid-stride 验证正确性 (host 端抽查) ----
    const int n = 1 << 20;
    float *da, *db, *dc, *hc = new float[n];
    CUDA_CHECK(cudaMalloc(&da, n * 4)); CUDA_CHECK(cudaMalloc(&db, n * 4));
    CUDA_CHECK(cudaMalloc(&dc, n * 4));
    CUDA_CHECK(cudaMemset(da, 0, n * 4));              // 0.0f
    CUDA_CHECK(cudaMemset(db, 0, n * 4));
    vecAddStride<<<24, 256>>>(da, db, dc, n);          // 固定 24 block (=SM 数), 与 n 无关
    CUDA_CHECK(cudaMemcpy(hc, dc, n * 4, cudaMemcpyDeviceToHost));
    printf("grid-stride (24 固定 block) 验证: c[0]=%.1f c[last]=%.1f (应为 0)\n", hc[0], hc[n - 1]);

    cudaFree(d_m); cudaFree(d_out); cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] hc;
    return 0;
}

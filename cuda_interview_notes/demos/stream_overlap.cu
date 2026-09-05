// stream_overlap.cu — 08/09 题配套 demo：pinned vs pageable + 双流重叠 + event 计时
// 编译: nvcc -std=c++17 -O2 -arch=sm_89 stream_overlap.cu -o so && ./so
#include <cstdio>
#include <cuda_runtime.h>

#define CHECK(x) do { cudaError_t e = (x); if (e != cudaSuccess) { \
    printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); return 1; } } while (0)

__global__ void vec_kernel(float* d, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float x = d[i];
    #pragma unroll 4
    for (int k = 0; k < 512; ++k) x = x * 1.000001f + 0.5f;   // 加重计算量, 让"算"和"传"体量相当, 重叠效果可见
    d[i] = x;
}

double time_ms(cudaEvent_t a, cudaEvent_t b) {
    float ms; cudaEventElapsedTime(&ms, a, b); return (double)ms;
}

int main() {
    const int N = 64 * 1024 * 1024;                 // 256MB float
    const size_t bytes = N * sizeof(float);
    float *h_page, *h_pin, *d_buf;
    CHECK(cudaMallocHost(&h_pin, bytes));           // pinned: 页锁定
    h_page = (float*)malloc(bytes);                 // pageable: 普通 malloc
    for (int i = 0; i < N; ++i) h_pin[i] = h_page[i] = 1.0f;
    CHECK(cudaMalloc(&d_buf, bytes));

    cudaEvent_t t0, t1; CHECK(cudaEventCreate(&t0)); CHECK(cudaEventCreate(&t1));

    // ---- 实验 1: pageable 同步拷贝 ----
    CHECK(cudaEventRecord(t0));
    CHECK(cudaMemcpy(d_buf, h_page, bytes, cudaMemcpyHostToDevice));
    CHECK(cudaEventRecord(t1)); CHECK(cudaEventSynchronize(t1));
    printf("pageable  H2D 同步: %.2f ms (%.1f GB/s)\n", time_ms(t0, t1), bytes / time_ms(t0, t1) / 1e6);

    // ---- 实验 2: pinned 同步拷贝 (省驱动中转, 通常快 30-50%) ----
    CHECK(cudaEventRecord(t0));
    CHECK(cudaMemcpy(d_buf, h_pin, bytes, cudaMemcpyHostToDevice));
    CHECK(cudaEventRecord(t1)); CHECK(cudaEventSynchronize(t1));
    printf("pinned    H2D 同步: %.2f ms (%.1f GB/s)\n", time_ms(t0, t1), bytes / time_ms(t0, t1) / 1e6);

    // ---- 实验 3: 单流 (拷贝→计算→拷回, 串行) vs 双流 (两块重叠流水线) ----
    const int half = N / 2; const size_t hb = bytes / 2;
    cudaStream_t s1, s2; CHECK(cudaStreamCreate(&s1)); CHECK(cudaStreamCreate(&s2));
    dim3 blk(256); dim3 grid((half + blk.x - 1) / blk.x);

    // 单流: 传完再算, 算完再传
    CHECK(cudaEventRecord(t0));
    CHECK(cudaMemcpyAsync(d_buf, h_pin, bytes, cudaMemcpyHostToDevice, s1));
    vec_kernel<<<(N + blk.x - 1) / blk.x, blk, 0, s1>>>(d_buf, N);
    CHECK(cudaMemcpyAsync(h_pin, d_buf, bytes, cudaMemcpyDeviceToHost, s1));
    CHECK(cudaStreamSynchronize(s1));
    CHECK(cudaEventRecord(t1)); CHECK(cudaEventSynchronize(t1));
    printf("单流 (传→算→传 串行):      %.2f ms\n", time_ms(t0, t1));

    // 双流: 流 s1 处理前半块, 流 s2 处理后半块 —— 拷贝和计算重叠
    CHECK(cudaEventRecord(t0));
    CHECK(cudaMemcpyAsync(d_buf, h_pin, hb, cudaMemcpyHostToDevice, s1));         // 前半进
    CHECK(cudaMemcpyAsync(d_buf + half, h_pin + half, hb, cudaMemcpyHostToDevice, s2)); // 后半进
    vec_kernel<<<grid, blk, 0, s1>>>(d_buf, half);                                // s1 算前半 (与 s2 的传输重叠)
    vec_kernel<<<grid, blk, 0, s2>>>(d_buf + half, half);                         // s2 算后半 (与 s1 的传输重叠)
    CHECK(cudaMemcpyAsync(h_pin, d_buf, hb, cudaMemcpyDeviceToHost, s1));         // 前半出
    CHECK(cudaMemcpyAsync(h_pin + half, d_buf + half, hb, cudaMemcpyDeviceToHost, s2)); // 后半出
    CHECK(cudaStreamSynchronize(s1)); CHECK(cudaStreamSynchronize(s2));
    CHECK(cudaEventRecord(t1)); CHECK(cudaEventSynchronize(t1));
    printf("双流 (拷贝/计算 重叠):     %.2f ms  ← 这就是你论文在系统级做的事的微缩版\n", time_ms(t0, t1));

    // 收尾
    cudaStreamDestroy(s1); cudaStreamDestroy(s2);
    cudaEventDestroy(t0); cudaEventDestroy(t1);
    cudaFreeHost(h_pin); free(h_page); cudaFree(d_buf);
    return 0;
}

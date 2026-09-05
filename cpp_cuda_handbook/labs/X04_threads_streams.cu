// X04 实验: 生产者(host 预取) + GPU 计算 的双线程流水线 vs 串行对照
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 X04_threads_streams.cu -o x04 && ./x04
#include <cstdio>
#include <thread>
#include <atomic>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

__global__ void heavyKernel(float* d, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float x = d[i];
    for (int k = 0; k < 128; ++k) x = x * 1.0001f + 0.2f;
    d[i] = x;
}

int main() {
    const int BATCH = 4, N = 8 * 1024 * 1024;            // 4 批 × 8M float (32MB/批)
    const size_t bytes = (size_t)N * sizeof(float);
    float *h;                                            // pinned 双缓冲
    CUDA_CHECK(cudaMallocHost(&h, 2 * bytes));
    float *d;
    CUDA_CHECK(cudaMalloc(&d, 2 * bytes));
    cudaStream_t sCopy, sCalc;
    CUDA_CHECK(cudaStreamCreate(&sCopy));
    CUDA_CHECK(cudaStreamCreate(&sCalc));
    cudaEvent_t t0, t1;
    CUDA_CHECK(cudaEventCreate(&t0)); CUDA_CHECK(cudaEventCreate(&t1));

    // ---- 对照: 串行 (CPU 准备 + 上传 + 计算 全在一条时间线上) ----
    auto prep = [&](int b) { for (int i = 0; i < N; ++i) h[(b % 2) * N + i] = (float)(b + i); };
    CUDA_CHECK(cudaEventRecord(t0));
    for (int b = 0; b < BATCH; ++b) {
        prep(b);                                        // CPU 准备数据(耗 CPU)
        CUDA_CHECK(cudaMemcpyAsync(d + (b % 2) * N, h + (b % 2) * N, bytes, cudaMemcpyHostToDevice, sCopy));
        CUDA_CHECK(cudaStreamSynchronize(sCopy));
        heavyKernel<<<(N + 255) / 256, 256, 0, sCalc>>>(d + (b % 2) * N, N);
        CUDA_CHECK(cudaStreamSynchronize(sCalc));
    }
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float msSer; cudaEventElapsedTime(&msSer, t0, t1);
    printf("串行版 (准备→传→算 依次来): %.2f ms\n", msSer);

    // ---- 流水线: 生产者线程预取第 i+1 批, GPU 同时算第 i 批 ----
    std::atomic<int> ready{0};
    CUDA_CHECK(cudaEventRecord(t0));
    std::thread producer([&] {
        for (int b = 0; b < BATCH; ++b) {
            prep(b);                                    // CPU 准备 (与 GPU 计算并行!)
            CUDA_CHECK(cudaMemcpyAsync(d + (b % 2) * N, h + (b % 2) * N, bytes,
                                       cudaMemcpyHostToDevice, sCopy));
            CUDA_CHECK(cudaStreamSynchronize(sCopy));   // 确保上传完成
            ready.fetch_add(1);
        }
    });
    int done_cnt = 0;
    while (done_cnt < BATCH) {
        while (ready.load() <= done_cnt) std::this_thread::yield();   // 等预取
        heavyKernel<<<(N + 255) / 256, 256, 0, sCalc>>>(d + (done_cnt % 2) * N, N);
        CUDA_CHECK(cudaStreamSynchronize(sCalc));
        ++done_cnt;
    }
    producer.join();
    CUDA_CHECK(cudaEventRecord(t1)); CUDA_CHECK(cudaEventSynchronize(t1));
    float msPipe; cudaEventElapsedTime(&msPipe, t0, t1);
    printf("流水线版 (CPU 预取 ∥ GPU 计算): %.2f ms  ← 提速 %.0f%%\n",
           msPipe, (1 - msPipe / msSer) * 100);

    cudaStreamDestroy(sCopy); cudaStreamDestroy(sCalc);
    cudaFreeHost(h); cudaFree(d);
    return 0;
}

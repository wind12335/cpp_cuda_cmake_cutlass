// G05 实验: 三种并行求和 —— block 树形规约 / warp shuffle 版 / atomic 版
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G05_reduce.cu -o g05 && ./g05
#include <cstdio>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

// ---------- 最终汇总 kernel ----------
// ⚠ 初版踩坑实录: 直接用 blockSumNaive<<<1,256>>> 汇总 16384 个部分和,
//   结果只有 1/64 —— 256 个线程只加了前 256 个数!
//   正确姿势: 每线程先用【循环预累加】扫完全部部分和(这正是 reduce 阶梯的第⑤级),
//   再做树形归约。16384/256 = 每线程 64 个数。
__global__ void sumFinal(const float* partial, float* out, int n) {
    __shared__ float smem[256];
    int tid = threadIdx.x;
    float v = 0.0f;
    for (int i = tid; i < n; i += blockDim.x) v += partial[i];   // ⑤ 预累加
    smem[tid] = v;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) smem[tid] += smem[tid + s];
        __syncthreads();
    }
    if (tid == 0) *out = smem[0];
}

// ---------- 版本 A: shared memory 树形规约 (面试手写及格线) ----------
__global__ void blockSumNaive(const float* in, float* out, int n) {
    __shared__ float smem[256];
    int tid = threadIdx.x;
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    smem[tid] = (i < n) ? in[i] : 0.0f;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) smem[tid] += smem[tid + s];
        __syncthreads();
    }
    if (tid == 0) out[blockIdx.x] = smem[0];
}

// ---------- 版本 B: warp shuffle 版 (进阶) ----------
__inline__ __device__ float warpReduce(float v) {
    for (int offset = 16; offset > 0; offset /= 2)
        v += __shfl_down_sync(0xffffffff, v, offset);   // 寄存器直接换手, 不过 shared
    return v;
}
__global__ void blockSumShuffle(const float* in, float* out, int n) {
    __shared__ float warp_sums[8];                       // 256/32 = 8 个 warp 的结果
    int tid = threadIdx.x, i = blockIdx.x * blockDim.x + threadIdx.x;
    float v = (i < n) ? in[i] : 0.0f;
    v = warpReduce(v);                                   // warp 内归约 (无 shared 无同步)
    if (tid % 32 == 0) warp_sums[tid / 32] = v;          // 每 warp 的 0 号线程写出
    __syncthreads();
    if (tid < 8) v = warp_sums[tid]; else v = 0.0f;      // 第一个 warp 再归约 8 个数
    v = warpReduce(v);
    if (tid == 0) out[blockIdx.x] = v;
}

// ---------- 版本 C: atomic 直接累加 ----------
__global__ void blockSumAtomic(const float* in, float* total, int n) {
    __shared__ float smem[256];
    int tid = threadIdx.x, i = blockIdx.x * blockDim.x + threadIdx.x;
    smem[tid] = (i < n) ? in[i] : 0.0f;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) smem[tid] += smem[tid + s];
        __syncthreads();
    }
    if (tid == 0) atomicAdd(total, smem[0]);             // 每 block 代表原子累加
}

int main() {
    const int N = 1 << 22;                               // 4M 个 1.0 → 总和应为 4194304
    const int BLOCK = 256, GRID = (N + BLOCK - 1) / BLOCK;
    float *d_in, *d_partial;
    float* d_total;
    CUDA_CHECK(cudaMalloc(&d_in, N * 4));
    CUDA_CHECK(cudaMalloc(&d_partial, GRID * 4));
    CUDA_CHECK(cudaMalloc(&d_total, 4));
    // 全部初始化为 1.0
    float* ones = new float[N];
    for (int i = 0; i < N; ++i) ones[i] = 1.0f;
    CUDA_CHECK(cudaMemcpy(d_in, ones, N * 4, cudaMemcpyHostToDevice));
    delete[] ones;
    float zero = 0.0f;
    CUDA_CHECK(cudaMemcpy(d_total, &zero, 4, cudaMemcpyHostToDevice));

    // A: 两段式
    blockSumNaive<<<GRID, BLOCK>>>(d_in, d_partial, N);
    sumFinal<<<1, BLOCK>>>(d_partial, d_total, GRID);        // 第二个 kernel 汇总
    float rA; CUDA_CHECK(cudaMemcpy(&rA, d_total, 4, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(d_total, &zero, 4, cudaMemcpyHostToDevice));   // 清零复用

    // B: shuffle 版 (同样两段式)
    blockSumShuffle<<<GRID, BLOCK>>>(d_in, d_partial, N);
    sumFinal<<<1, BLOCK>>>(d_partial, d_total, GRID);
    float rB; CUDA_CHECK(cudaMemcpy(&rB, d_total, 4, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(d_total, &zero, 4, cudaMemcpyHostToDevice));

    // C: atomic 版 (单 kernel)
    blockSumAtomic<<<GRID, BLOCK>>>(d_in, d_total, N);
    float rC; CUDA_CHECK(cudaMemcpy(&rC, d_total, 4, cudaMemcpyDeviceToHost));

    printf("期望总和 = %d\n", N);
    printf("A naive  树形规约 : %.0f %s\n", rA, rA == N ? "✓" : "✗");
    printf("B shuffle 版      : %.0f %s\n", rB, rB == N ? "✓" : "✗");
    printf("C atomic 版       : %.0f %s  (顺序不定, 数值可能有 ±个位数误差)\n", rC, rC == N ? "✓" : "≈");

    cudaFree(d_in); cudaFree(d_partial); cudaFree(d_total);
    return 0;
}

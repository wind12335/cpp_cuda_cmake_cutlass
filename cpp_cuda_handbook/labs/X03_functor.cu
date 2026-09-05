// X03 实验: 泛型 transform kernel —— 你自己的 mini-Thrust
// ⚠ 编译命令含 --extended-lambda: 带 __device__ 标记的 lambda 需要它(不加编译报错, 亲测)
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 --extended-lambda X03_functor.cu -o x03 && ./x03
#include <cstdio>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

// §X03.2 仿函数: device 端可调用
struct Scale {
    float k;
    explicit Scale(float k) : k(k) {}
    __device__ float operator()(float x) const { return x * k; }
};
struct ReLU {
    __device__ float operator()(float x) const { return x > 0 ? x : 0; }
};

// §X03.3 一元 transform (grid-stride)
template <class F>
__global__ void transformKernel(const float* in, float* out, int n, F f) {
    int stride = gridDim.x * blockDim.x;
    for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += stride)
        out[i] = f(in[i]);
}

// §X03.5 二元 zipWith
template <class F>
__global__ void zipWith(const float* a, const float* b, float* c, int n, F f) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = f(a[i], b[i]);
}

template <class F>
void transformGPU(float* d, int n, F f) {
    int block = 256, grid = (n + block - 1) / block;
    transformKernel<<<grid, block>>>(d, d, n, f);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
}

int main() {
    const int n = 1 << 16;
    float *da, *db, *dc, *h = new float[n];
    CUDA_CHECK(cudaMalloc(&da, n * 4));
    CUDA_CHECK(cudaMalloc(&db, n * 4));
    CUDA_CHECK(cudaMalloc(&dc, n * 4));
    CUDA_CHECK(cudaMemset(da, 0, n * 4));
    for (int i = 0; i < n; ++i) { float v = (float)(i % 64) - 32.0f;   // 有正有负(测 ReLU)
        CUDA_CHECK(cudaMemcpy(da + i, &v, 4, cudaMemcpyHostToDevice)); }

    // ---- 一元: 仿函数 ----
    transformGPU(da, n, Scale(2.0f));
    CUDA_CHECK(cudaMemcpy(h, da, n * 4, cudaMemcpyDeviceToHost));
    printf("Scale(2) 后 h[33]=%.1f (期望 2.0)\n", h[33]);    // 33%64-32=1 → *2=2

    // ---- 一元: lambda (CUDA 11 扩展 lambda) ----
    transformGPU(da, n, [] __device__ (float x) { return x > 0 ? x : 0; });   // ReLU
    CUDA_CHECK(cudaMemcpy(h, da, n * 4, cudaMemcpyDeviceToHost));
    printf("ReLU 后 h[33]=%.1f, h[1]=%.1f (负数应为 0)\n", h[33], h[1]);

    // ---- 二元: zipWith 加法 ----
    transformGPU(da, n, Scale(1.0f));                        // 恢复
    zipWith<<<(n + 255) / 256, 256>>>(da, da, dc, n,
        [] __device__ (float x, float y) { return x + y; }); // c = a + a
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaMemcpy(h, dc, n * 4, cudaMemcpyDeviceToHost));
    printf("zipWith 自加后 h[33]=%.1f (期望 4.0)\n", h[33]);

    cudaFree(da); cudaFree(db); cudaFree(dc); delete[] h;
    return 0;
}

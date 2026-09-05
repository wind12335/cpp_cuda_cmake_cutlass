#include "mykernels.cuh"     // 前置: K03.3 依赖传染——链接 mykernels 后自动可见
#include <vector>
#include <cuda_runtime.h>

int main() {
    std::vector<float> v(1000, 2.0f);
    float* d;
    cudaMalloc(&d, v.size() * 4);
    cudaMemcpy(d, v.data(), v.size() * 4, cudaMemcpyHostToDevice);
    printf("GPU 求和 = %.0f (期望 2000)\n", gpu_sum(d, (int)v.size()));
    cudaFree(d);
    return 0;
}

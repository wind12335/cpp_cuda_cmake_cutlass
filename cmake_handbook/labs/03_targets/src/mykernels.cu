#include "mykernels.cuh"     // 库自己的头文件(前置: K03 PUBLIC include 的演示)
#include <cuda_runtime.h>

__global__ void sumKernel(const float* d, float* out, int n) {
    __shared__ float smem[256];
    int tid = threadIdx.x, i = blockIdx.x * blockDim.x + threadIdx.x;
    smem[tid] = (i < n) ? d[i] : 0.0f;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) smem[tid] += smem[tid + s];
        __syncthreads();
    }
    if (tid == 0) atomicAdd(out, smem[0]);      // 规约(前置: cuda_handbook G05)
}

float gpu_sum(const float* d, int n) {
    float* d_total; float h = 0;
    cudaMalloc(&d_total, 4); cudaMemset(d_total, 0, 4);
    sumKernel<<<(n + 255) / 256, 256>>>(d, d_total, n);
    cudaMemcpy(&h, d_total, 4, cudaMemcpyDeviceToHost);
    cudaFree(d_total);
    return h;
}

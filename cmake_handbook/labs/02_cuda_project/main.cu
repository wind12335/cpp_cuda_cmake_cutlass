// 向量加法: CMake 构建的 CUDA 工程入口 (同 cuda_handbook G02)
#include <cstdio>
#include <cuda_runtime.h>

__global__ void vecAdd(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

int main() {
    int n = 1 << 20;
    float *da, *db, *dc, *ha = new float[n], *hb = new float[n], *hc = new float[n];
    for (int i = 0; i < n; ++i) { ha[i] = 1.0f; hb[i] = 2.0f; }
    cudaMalloc(&da, n * 4); cudaMalloc(&db, n * 4); cudaMalloc(&dc, n * 4);
    cudaMemcpy(da, ha, n * 4, cudaMemcpyHostToDevice);
    cudaMemcpy(db, hb, n * 4, cudaMemcpyHostToDevice);
    vecAdd<<<(n + 255) / 256, 256>>>(da, db, dc, n);
    cudaMemcpy(hc, dc, n * 4, cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    printf("CMake 构建: c[0]=%.1f c[last]=%.1f\n", hc[0], hc[n - 1]);
    cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] ha; delete[] hb; delete[] hc;
    return 0;
}

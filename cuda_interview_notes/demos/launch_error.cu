// launch_error.cu — 10 题配套 demo：CUDA 错误的"滞后、漏报、毒化"三大特性
// 编译: nvcc -std=c++17 -arch=sm_89 launch_error.cu -o le && ./le
// 定位: compute-sanitizer --tool memcheck ./le   ← 对本程序会报告 2 处 Invalid write
#include <cstdio>
#include <cuda_runtime.h>

// 越界 kernel: offset 控制"小越界(漏报) / 大越界(当场炸)"
__global__ void buggy_kernel(float* d, int n, long offset) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i == 0) d[offset] = 1.0f;                  // ← offset >= n 即越界
    if (i < n) d[i] *= 2.0f;
}

void report(const char* tag, cudaError_t e) {
    printf("  %-28s : %s\n", tag, cudaGetErrorString(e));
}

int main() {
    const int N = 1024;                            // 4KB 显存
    float* d;
    if (cudaMalloc(&d, N * sizeof(float)) != cudaSuccess) return 1;

    printf("=== 剧情 1: 小越界 (d[%d], 只越了 100 个 float) ===\n", N + 100);
    buggy_kernel<<<1, 128>>>(d, N, N + 100);
    report("launch 后 cudaGetLastError", cudaGetLastError());
    report("cudaDeviceSynchronize", cudaDeviceSynchronize());
    printf("  → 两次都是 no error! 小越界落在分配粒度的空隙里, 静默损坏数据。\n");
    printf("  → 错误码完全漏报, 只有 compute-sanitizer 能抓到 (跑法见文件头)。\n\n");

    printf("=== 剧情 2: 大越界 (d[N*10000], 远超页边界) ===\n");
    buggy_kernel<<<1, 128>>>(d, N, (long)N * 10000);
    report("launch 后 cudaGetLastError", cudaGetLastError());   // launch 本身合法, 这里仍 no error
    report("cudaDeviceSynchronize", cudaDeviceSynchronize());   // 异步错误在这里冒出来
    printf("  → launch 时没事, sync 才报 'illegal memory access': 错误是异步滞后的。\n\n");

    printf("=== 剧情 3: context 毒化 ===\n");
    float* d2;
    report("之后随便一个 cudaMalloc", cudaMalloc(&d2, 16));
    printf("  → 不是这个调用有问题: 错误挂起后, 同一 context 的所有后续 CUDA 调用全失败。\n\n");

    printf("排查路径: ① 每次 launch 后查 cudaGetLastError ② CUDA_LAUNCH_BLOCKING=1 归位错误\n");
    printf("          ③ compute-sanitizer --tool memcheck 精确到行 ④ nsys 看时间线\n");
    cudaFree(d);   // context 已毒化, 这一步也会失败 —— 亲手试试
    return 0;
}

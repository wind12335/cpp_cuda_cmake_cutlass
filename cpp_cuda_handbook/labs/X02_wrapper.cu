// X02 实验: kernel 启动包装器 —— 错误必查 + 类型安全
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 X02_wrapper.cu -o x02 && ./x02
#include <cstdio>
#include <stdexcept>
#include <string>
#include <cuda_runtime.h>

// 异常风格的错误处理: 一处抛, RAII 析构负责清理
#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) \
        throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(e) + \
                                 " @ " #x " (line " + std::to_string(__LINE__) + ")"); \
} while (0)

__global__ void vecAdd(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

// §X02.2 启动包装器: 完美转发参数 + launch 后必查错误
template <class Kernel, class... Args>
void launch(Kernel k, dim3 grid, dim3 block, Args&&... args) {
    k<<<grid, block>>>(std::forward<Args>(args)...);
    cudaError_t e = cudaGetLastError();
    if (e != cudaSuccess)
        throw std::runtime_error(std::string("kernel launch 失败: ") + cudaGetErrorString(e));
}

// §X02.3 grid 计算函数
dim3 grid1d(int n, int block = 256) { return dim3((n + block - 1) / block); }

// §X02.4 配置对象版
struct LaunchConfig {
    dim3 grid, block;
    size_t shmem = 0;
    cudaStream_t stream = nullptr;
};
template <class Kernel, class... Args>
void launchConfigured(Kernel k, LaunchConfig c, Args&&... args) {
    k<<<c.grid, c.block, c.shmem, c.stream>>>(std::forward<Args>(args)...);
    CUDA_CHECK(cudaGetLastError());
}

int main() {
    const int n = 1 << 16;
    float *da, *db, *dc, *hc = new float[n];
    CUDA_CHECK(cudaMalloc(&da, n * 4));
    CUDA_CHECK(cudaMalloc(&db, n * 4));
    CUDA_CHECK(cudaMalloc(&dc, n * 4));
    CUDA_CHECK(cudaMemset(da, 0, n * 4));
    CUDA_CHECK(cudaMemset(db, 0, n * 4));

    // ---- 1. 基础包装器 ----
    launch(vecAdd, grid1d(n), dim3(256), da, db, dc, n);
    CUDA_CHECK(cudaMemcpy(hc, dc, n * 4, cudaMemcpyDeviceToHost));
    printf("包装器版: c[100]=%.1f\n", hc[100]);

    // ---- 2. 配置对象版 ----
    LaunchConfig cfg;
    cfg.grid = grid1d(n); cfg.block = dim3(128);
    launchConfigured(vecAdd, cfg, da, db, dc, n);
    printf("配置对象版也通过 (错误已自动检查)\n");

    // ---- 3. 故意给错误配置: 看"错误必查"的效果 ----
    try {
        launch(vecAdd, dim3(1), dim3(2000), da, db, dc, n);   // 2000 > 1024 上限!
    } catch (const std::runtime_error& e) {
        printf("错误被抓住: %s\n", e.what());
    }
    printf("↑ 不用包装器的话, 这种配置错误会被静默吞掉, 结果全错你也发现不了\n");

    cudaFree(da); cudaFree(db); cudaFree(dc); delete[] hc;
    return 0;
}

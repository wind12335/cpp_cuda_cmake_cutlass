// X01 实验: 显存的 RAII 封装 —— DeviceBuffer / shared_ptr 方案 / 容器化
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 X01_device_buffer.cu -o x01 && ./x01
#include <cstdio>
#include <memory>
#include <vector>
#include <stdexcept>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) { printf("CUDA error %s @%d: %s\n", #x, __LINE__, cudaGetErrorString(e)); exit(1);} } while (0)

// ─── 方案一: RAII 类 (Rule of Five 完整体) ───
class DeviceBuffer {
public:
    explicit DeviceBuffer(size_t n) : size_(n) {
        CUDA_CHECK(cudaMalloc(&ptr_, n));
        printf("  [alloc] %zu bytes @ %p\n", n, (void*)ptr_);
    }
    ~DeviceBuffer() {
        if (ptr_) { printf("  [free ] %zu bytes (RAII 自动)\n", size_); cudaFree(ptr_); }
    }
    DeviceBuffer(const DeviceBuffer&) = delete;              // 禁拷贝
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    DeviceBuffer(DeviceBuffer&& o) noexcept                  // 可移动(noexcept 是 vector 的要求)
        : ptr_(o.ptr_), size_(o.size_) { o.ptr_ = nullptr; }
    DeviceBuffer& operator=(DeviceBuffer&& o) noexcept {
        if (this != &o) { if (ptr_) cudaFree(ptr_);
                          ptr_ = o.ptr_; size_ = o.size_; o.ptr_ = nullptr; }
        return *this;
    }
    float* get() const { return ptr_; }
    size_t size() const { return size_; }
private:
    float* ptr_ = nullptr;
    size_t size_ = 0;
};

// ─── 方案二: shared_ptr + 自定义删除器 (一行流) ───
static std::shared_ptr<float> makeDeviceFloat(size_t n) {
    float* p = nullptr;
    CUDA_CHECK(cudaMalloc(&p, n * sizeof(float)));
    return std::shared_ptr<float>(p, [](float* q) {          // 删除器 = cudaFree
        if (q) cudaFree(q);
    });
}

__global__ void fillKernel(float* d, int n, float v) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) d[i] = v;
}

int main() {
    printf("=== RAII 类: 异常路径也不泄漏 ===\n");
    try {
        DeviceBuffer buf(16 * sizeof(float));
        fillKernel<<<1, 16>>>(buf.get(), 16, 7.0f);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        throw std::runtime_error("模拟中途出错!");            // 异常抛出!
    } catch (const std::exception& e) {
        printf("  捕获: %s\n", e.what());
    }
    printf("  ↑ 上面的 [free] 打印了: RAII 在异常路径也自动释放\n");

    printf("\n=== shared_ptr 方案 ===\n");
    auto d = makeDeviceFloat(8);
    fillKernel<<<1, 8>>>(d.get(), 8, 3.0f);
    CUDA_CHECK(cudaDeviceSynchronize());
    printf("  引用计数=%ld (拷贝共享, 自动 cudaFree)\n", d.use_count());

    printf("\n=== 容器化: 一批缓冲统一生命周期 ===\n");
    {
        std::vector<DeviceBuffer> bufs;
        bufs.emplace_back(4 * sizeof(float));
        bufs.emplace_back(8 * sizeof(float));
        bufs.push_back(std::move(bufs[0]));                  // 移动语义(前置 C07)
        printf("  bufs 共 %zu 个缓冲\n", bufs.size());
    }                                                        // 全部自动释放
    printf("  离开作用域: 全部 [free] 打印 = 零泄漏\n");
    return 0;
}

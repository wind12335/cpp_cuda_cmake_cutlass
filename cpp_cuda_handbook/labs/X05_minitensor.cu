// X05 毕业项目: mini GPU 张量类 —— C++ RAII/移动/模板 × CUDA kernel 的端到端集成
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 X05_minitensor.cu -o x05 && ./x05
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <cuda_runtime.h>

#define CUDA_CHECK(x) do { cudaError_t e = (x); \
    if (e != cudaSuccess) \
        throw std::runtime_error(std::string("CUDA: ") + cudaGetErrorString(e) + " @ " #x); \
} while (0)

// ───────── kernel 侧 (前置 G02/G05/X03) ─────────
__global__ void fillKernel(float* d, int n, float v) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) d[i] = v;
}
__global__ void scaleKernel(const float* in, float* out, int n, float k) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) out[i] = in[i] * k;
}
__global__ void addKernel(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}
__global__ void sumKernelAtomic(const float* in, float* total, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) atomicAdd(total, in[i]);        // 简化版: 逐元素原子累加(教学用)
}

// ───────── host 侧: Tensor 类 ─────────
class Tensor {
public:
    explicit Tensor(size_t n) : n_(n) { CUDA_CHECK(cudaMalloc(&d_, n * sizeof(float))); }
    Tensor(const Tensor&) = delete;                          // 禁拷贝(双释放不可能)
    Tensor(Tensor&& o) noexcept : d_(o.d_), n_(o.n_) { o.d_ = nullptr; o.n_ = 0; }
    Tensor& operator=(Tensor&& o) noexcept {
        if (this != &o) { if (d_) cudaFree(d_); d_ = o.d_; n_ = o.n_; o.d_ = nullptr; o.n_ = 0; }
        return *this;
    }
    ~Tensor() { if (d_) cudaFree(d_); }

    void fill(float v) {
        fillKernel<<<(n_ + 255) / 256, 256>>>(d_, (int)n_, v);
        CUDA_CHECK(cudaGetLastError());
    }
    Tensor scale(float k) const {
        Tensor out(n_);
        scaleKernel<<<(n_ + 255) / 256, 256>>>(d_, out.d_, (int)n_, k);
        CUDA_CHECK(cudaGetLastError());
        return out;                                          // RVO/移动: 零多余拷贝(别写 std::move!)
    }
    Tensor operator+(const Tensor& o) const {
        Tensor out(n_);
        addKernel<<<(n_ + 255) / 256, 256>>>(d_, o.d_, out.d_, (int)n_);
        CUDA_CHECK(cudaGetLastError());
        return out;
    }
    float sum() const {
        float* d_total; float h_total = 0.0f;
        CUDA_CHECK(cudaMalloc(&d_total, 4));
        CUDA_CHECK(cudaMemset(d_total, 0, 4));
        sumKernelAtomic<<<(n_ + 255) / 256, 256>>>(d_, d_total, (int)n_);
        CUDA_CHECK(cudaMemcpy(&h_total, d_total, 4, cudaMemcpyDeviceToHost));
        cudaFree(d_total);
        return h_total;
    }
    std::vector<float> toHost() const {
        std::vector<float> h(n_);
        CUDA_CHECK(cudaMemcpy(h.data(), d_, n_ * sizeof(float), cudaMemcpyDeviceToHost));
        return h;
    }
    size_t size() const { return n_; }
    float* get() const { return d_; }

private:
    float* d_;
    size_t n_;
};

int main() {
    printf("== 毕业演示: fill → scale → add → sum → toHost ==\n");
    Tensor t1(1 << 20);                              // 1M 元素
    t1.fill(1.0f);                                   // 全 1

    Tensor t2 = t1.scale(2.0f);                      // 全 2 (返回新张量, RVO 零拷贝)
    Tensor t3 = t1 + t2;                             // 全 3 (运算符重载)

    printf("t3.sum() = %.0f (期望 %d)\n", t3.sum(), 3 * (1 << 20));

    std::vector<float> host = t3.toHost();           // 搬回主机
    printf("host[0]=%.1f host[last]=%.1f, 容器大小=%zu\n",
           host[0], host.back(), host.size());

    // 移动语义: vector 里存张量(禁拷贝所以只能移动)
    std::vector<Tensor> pool;
    pool.push_back(std::move(t3));
    pool.push_back(Tensor(64));                      // 临时对象 → 移动
    printf("pool 有 %zu 个张量: %zu + %zu\n", pool.size(), pool[0].size(), pool[1].size());
    printf("\n离开 main: 全部显存由 RAII 自动释放, 零泄漏 ✓\n");
    return 0;
}

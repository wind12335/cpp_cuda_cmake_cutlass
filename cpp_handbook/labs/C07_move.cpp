// C07 实验: 移动语义
// 运行: g++ -std=c++17 -O2 C07_move.cpp -o c07 && ./c07
#include <cstdio>
#include <chrono>
#include <cstring>
#include <utility>
#include <vector>

class Buffer {
public:
    explicit Buffer(size_t n) : size_(n), data_(new char[n]) { puts("  构造"); }
    Buffer(const Buffer& o) : size_(o.size_), data_(new char[o.size_]) {
        std::memcpy(data_, o.data_, size_); puts("  拷贝构造 (O(n) 真复制)");
    }
    Buffer(Buffer&& o) noexcept : size_(o.size_), data_(o.data_) {
        o.data_ = nullptr; o.size_ = 0; puts("  移动构造 (O(1) 抢指针)");
    }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) { delete[] data_; data_ = o.data_; size_ = o.size_;
                          o.data_ = nullptr; o.size_ = 0; }
        return *this;
    }
    ~Buffer() { delete[] data_; }
    size_t size() const { return size_; }        // 公有只读接口(成员本身是 private)
private:
    size_t size_ = 0;
    char* data_ = nullptr;
};

int main() {
    // ---- 拷贝 vs 移动: 大缓冲计时的量级差 ----
    Buffer big(100u << 20);   // 100MB
    auto t0 = std::chrono::steady_clock::now();
    Buffer copy1 = big;                                    // 拷贝: 100MB memcpy
    auto t1 = std::chrono::steady_clock::now();
    Buffer moved = std::move(big);                         // 移动: 换指针
    auto t2 = std::chrono::steady_clock::now();
    printf("拷贝 100MB 耗时 %lld μs, 移动耗时 %lld ns\n",
           (long long)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
           (long long)std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
    printf("移动后 big.size=%zu (空壳), moved.size=%zu\n", big.size(), moved.size());
    // (成员是 private, 这里换种说法直接看行为) ↓ 用容量对比

    // ---- vector 扩容: 移动的价值 ----
    std::vector<Buffer> v;
    v.reserve(3);
    printf("push 3 个 Buffer 进 vector(已 reserve, 无扩容):\n");
    v.emplace_back(10); v.emplace_back(20); v.emplace_back(30);
    puts("  (都只构造一次 —— reserve 的价值, 前置 T1-11)");
    (void)copy1; (void)moved;
    return 0;
}

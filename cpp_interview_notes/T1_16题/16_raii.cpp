// 16 RAII: 资源生命周期绑定对象生命周期
// 运行: g++ -std=c++17 -pthread 16_raii.cpp -o t16 && ./t16
#include <cstdio>
#include <stdexcept>
#include <mutex>

// ---- 1. 锁的 RAII (标准库 lock_guard 的原理) ----
struct MyGuard {
    std::mutex& m_;
    explicit MyGuard(std::mutex& m) : m_(m) { m_.lock();   printf("  [guard] 加锁\n"); }
    ~MyGuard() { m_.unlock(); printf("  [guard] 解锁\n"); }
};

// ---- 2. CUDA 风格: 设备缓冲区的 RAII 封装 (拿 malloc / 析构 free) ----
struct DeviceBuffer {
    void* ptr = nullptr;
    size_t bytes;
    explicit DeviceBuffer(size_t b) : bytes(b) {
        ptr = std::malloc(b);              // 实际项目里是 cudaMalloc / hipMalloc
        printf("  [buffer] 分配 %zu bytes @ %p\n", bytes, ptr);
    }
    ~DeviceBuffer() {
        std::free(ptr);                    // 实际项目里是 cudaFree
        printf("  [buffer] 释放 %zu bytes\n", bytes);
    }
    DeviceBuffer(const DeviceBuffer&) = delete;             // 资源类禁拷贝
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};

void risky_work(DeviceBuffer&) {
    throw std::runtime_error("kernel 参数配置错误!");        // 中途抛异常
}

int main() {
    std::mutex m;
    printf("--- 正常路径: guard 随作用域解锁 ---\n");
    {
        MyGuard g(m);
        printf("  临界区工作中...\n");
    }
    printf("--- 异常路径: 栈展开时析构照样执行 ---\n");
    try {
        DeviceBuffer buf(1 << 20);
        MyGuard g(m);
        risky_work(buf);                    // 抛异常!
        printf("  这行不会执行\n");
    } catch (const std::exception& e) {
        printf("  捕获异常: %s\n", e.what());
    }
    // 看输出: buffer 释放、guard 解锁都发生了 —— 这就是 RAII 的价值
    // 手工管理在 throw 那行之前漏掉 free/unlock 的话, 上面的释放都不会发生
    return 0;
}

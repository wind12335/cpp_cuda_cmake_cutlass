# X01 显存管理的 RAII 封装 【核心】

> 目标：让"忘记 cudaFree"从代码里**不可能发生**。
> 【前置：C06 智能指针与 RAII】【CUDA G04.3 cudaMalloc 家族】

## §X01.1 问题：裸 cudaMalloc 的三大痛点

```cpp
float* d;
cudaMalloc(&d, bytes);
// ... 100 行代码, 中途 3 个 return、1 个 throw ...
cudaFree(d);      // 只要有一条路径漏掉 → 泄漏; 异常抛出更必然泄漏
```

痛点：①手动释放容易漏；②异常路径必漏；③"d_ptr 有没有被 free 过"全靠脑子记。

## §X01.2 方案一：RAII 类（禁拷贝、可移动）

**作用**：把显存绑进一个对象——构造时 cudaMalloc、析构时 cudaFree。

```cpp
class DeviceBuffer {
public:
    explicit DeviceBuffer(size_t n_bytes) : size_(n_bytes) {
        cudaMalloc(&ptr_, n_bytes);
    }
    ~DeviceBuffer() { if (ptr_) cudaFree(ptr_); }

    DeviceBuffer(const DeviceBuffer&) = delete;             // 禁拷贝(两对象管同一显存会双释放)
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    DeviceBuffer(DeviceBuffer&& o) noexcept                 // 移动: 转移所有权
        : ptr_(o.ptr_), size_(o.size_) { o.ptr_ = nullptr; }
    DeviceBuffer& operator=(DeviceBuffer&& o) noexcept {
        if (this != &o) { if (ptr_) cudaFree(ptr_);
                          ptr_ = o.ptr_; size_ = o.size_; o.ptr_ = nullptr; }
        return *this;
    }

    float* get() const { return ptr_; }                     // 给 kernel/cudaMemcpy 用
    size_t size() const { return size_; }
private:
    float* ptr_ = nullptr;
    size_t size_ = 0;
};
```

用法：`DeviceBuffer buf(bytes); kernel<<<...>>>(buf.get());` —— 无论从哪条路离开作用域，
显存自动释放。这就是 Rule of Five 的完整落地（前置：C05.2）。

## §X01.3 方案二：shared_ptr + 自定义删除器（一行流）

**作用**：用 std 的账本机制管显存——deleter 换成 cudaFree 即可（前置：T1-04 deleter 概念）。

```cpp
std::shared_ptr<float> d(
    [&]{ float* p; cudaMalloc(&p, bytes); return p; }(),   // 立即调 lambda 分配
    [](float* p) { cudaFree(p); });                        // 自定义删除器: cudaFree
// 拷贝 shared → 计数+1; 最后一个销毁时自动 cudaFree —— 多处共享显存时的优雅解
```

## §X01.4 host 侧 pinned 内存同理

```cpp
auto h = std::shared_ptr<float>(
    [&]{ float* p; cudaMallocHost(&p, bytes); return p; }(),
    [](float* p) { cudaFreeHost(p); });
```

## §X01.5 容器化：vector<DeviceBuffer> 与移动语义

```cpp
std::vector<DeviceBuffer> bufs;
bufs.emplace_back(bytes);          // 直接在容器内构造(需要可移动, 前置 C07 noexcept 的意义)
bufs.push_back(std::move(buf));    // 移动进容器
// 容器析构 → 全部显存自动释放 —— "一批缓冲的统一生命周期管理"零心智负担
```

【坑】vector 扩容搬移元素时只敢用 noexcept 移动——**移动构造忘记标 noexcept** 会导致
拷贝失败直接编译报错（unique_ptr 不可拷贝，vector 退无可退）。

## 本篇实验

`labs/X01_device_buffer.cu`——DeviceBuffer 全生命周期演示 + shared_ptr 方案 + 容器化。

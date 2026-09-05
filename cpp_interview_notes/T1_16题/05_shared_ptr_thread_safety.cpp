// 05 shared_ptr 线程安全的三层区分
// 运行: g++ -std=c++17 -pthread 05_shared_ptr_thread_safety.cpp -o t5 && ./t5
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>

struct Data {
    int value = 0;
    explicit Data(int v) : value(v) {}
};

int main() {
    // ---- 第 1 层: 引用计数原子 → 各线程拷贝各自的 shared_ptr，完全安全 ----
    auto shared = std::make_shared<Data>(42);
    std::vector<std::thread> pool;
    for (int i = 0; i < 8; ++i) {
        pool.emplace_back([shared] {          // 按值捕获 = 每个线程自己的拷贝，计数原子加减
            if (shared->value != 42) printf("impossible\n");
        });
    }
    for (auto& t : pool) t.join();
    printf("8 线程各持拷贝，安全, use_count 回到 %ld\n", shared.use_count());

    // ---- 第 2 层: 对象本身不受保护 → 并发写 *p 是数据竞争（此处仅示意，串行执行） ----
    {
        auto d = std::make_shared<Data>(0);
        std::mutex m;
        auto writer = [&m, d] { std::lock_guard<std::mutex> lk(m); d->value++; };  // 对象访问要自己上锁
        std::thread t1(writer), t2(writer);
        t1.join(); t2.join();
        printf("对象访问加锁后 value=%d\n", d->value);
    }

    // ---- 第 3 层: 同一个 shared_ptr 实例被多线程写 → 未定义行为 ----
    // std::shared_ptr<Data> p = std::make_shared<Data>(1);
    // std::thread t1([&] { p = std::make_shared<Data>(2); });   // UB: 并发写 p 本身!
    // std::thread t2([&] { auto q = p; });                      // UB: 与写并发地拷贝 p
    // C++20 的解法: std::atomic<std::shared_ptr<Data>> ap;  或者直接上 mutex
    printf("第 3 层见注释 —— 真跑就是数据竞争, 用 mutex 或 atomic<shared_ptr> 解决\n");
    return 0;
}

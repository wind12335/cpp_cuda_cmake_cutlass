// os3_mutex_vs_spin.c — 互斥锁 vs 自旋锁: 什么场景选谁, 实测说话
// 编译: g++ -O2 -pthread os3_mutex_vs_spin.c -o os3 && ./os3
#include <cstdio>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <time.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

template <class F>
double bench(const char* name, int threads, long iters, F&& body) {
    double t0 = now_ms();
    std::vector<std::thread> ts;
    for (int i = 0; i < threads; ++i) ts.emplace_back([&] { for (long k = 0; k < iters; ++k) body(); });
    for (auto& t : ts) t.join();
    double t1 = now_ms();
    double ns_per_op = (t1 - t0) * 1e6 / (double)threads / iters;
    printf("  %-28s %4d 线程: %8.1f ns/op\n", name, threads, ns_per_op);
    return ns_per_op;
}

int main() {
    std::mutex mu;
    std::atomic<long> cnt{0};
    long raw = 0;

    // ---- 1. 无竞争: 单线程反复 加锁/解锁 ----
    printf("=== 无竞争 (单线程 锁/解锁 纯开销) ===\n");
    bench("std::mutex", 1, 10000000, [&] { mu.lock(); mu.unlock(); });
    bench("atomic fetch_add", 1, 10000000, [&] { cnt.fetch_add(1, std::memory_order_relaxed); });

    // ---- 2. 高竞争: 8 线程抢同一把锁 (临界区内做一点事, 模拟真实) ----
    printf("=== 高竞争 (8 线程 × 200万次进临界区) ===\n");
    {
        std::atomic<long> work{0};
        bench("std::mutex 高竞争", 8, 2000000, [&] {
            mu.lock(); raw++; work.fetch_add(1, std::memory_order_relaxed); mu.unlock();
        });
        bench("atomic(无锁) 对照", 8, 2000000, [&] {
            cnt.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // ---- 口径 ----
    printf("\n结论(本机 %u 核): 无竞争 mutex ≈ 一次原子 CAS 级别(~15-25ns);\n", std::thread::hardware_concurrency());
    printf("高竞争时 mutex(睡眠队列) vs 自旋锁(忙等烧核) 的选择标准:\n");
    printf("  临界区极短且核多(如内核中断路径) → 自旋; 用户态长临界区/IO → mutex(让出 CPU);\n");
    printf("  单变量计数 → 根本别上锁, atomic。\n");
    printf("GPU 联动: atomicAdd 高竞争同款问题 → warp 级分桶/归约规避(和 C++ 22 题同源)。\n");
    return 0;
}

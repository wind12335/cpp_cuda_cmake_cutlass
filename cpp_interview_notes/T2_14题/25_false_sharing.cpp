// 25 false sharing: 同缓存行互相打脸 —— md 名词 ↔ 代码实体对照:
//   "64B 缓存行(一条船)"   → CPU 核间同步内存的最小单位, sizeof(long)*8 挤在一两条船里
//   "Packed(挤一条船)"     → 8 个 long 紧挨着: 逻辑上 8 线程各写各的, 物理上同一缓存行
//   "Padded(一人一条船)"   → 每个槽 alignas(64): 独占缓存行, 写操作互不干扰
//   "数据无关但性能差几倍"  → 两个 run 的唯一区别是内存布局, 校验和都应=160000000
// 运行: g++ -std=c++17 -O2 -pthread 25_false_sharing.cpp -o t25 && ./t25
#include <cstdio>
#include <thread>
#include <vector>
#include <chrono>

constexpr int N_THREADS = 8;
constexpr long N_ADDS   = 20'000'000;

// 关键设计: 每个线程只写"自己的"槽位 → 没有数据竞争, 结果可校验;
// 两个版本唯一的区别是槽位的内存布局 (是否共享缓存行)
struct Packed {                    // 8 个 long 紧挨着: 8 槽挤在 1~2 条 64B 缓存行上
    long v[N_THREADS] = {};
};
struct Slot   { alignas(64) long v = 0; };   // 一槽独占一条缓存行
struct Padded {
    Slot s[N_THREADS];
};

template <class S>
double run(const char* name) {
    S s;
    std::vector<std::thread> ts;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N_THREADS; ++i) {
        ts.emplace_back([&s, i] {
            for (long k = 0; k < N_ADDS; ++k) ++s_member(s, i);   // 各写各的槽位
        });
    }
    for (auto& t : ts) t.join();
    auto t1 = std::chrono::steady_clock::now();
    long sum = 0;
    for (int i = 0; i < N_THREADS; ++i) sum += get_slot(s, i);
    printf("  %s: 校验和=%ld %s\n", name, sum,
           sum == N_THREADS * N_ADDS ? "✓" : "✗ 数据竞争!");
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// 访问不同布局的槽位 (简单起见用两个特化)
long& s_member(Packed& s, int i) { return s.v[i]; }
long& s_member(Padded& s, int i) { return s.s[i].v; }
long  get_slot(const Packed& s, int i) { return s.v[i]; }
long  get_slot(const Padded& s, int i) { return s.s[i].v; }

int main() {
    printf("%d 线程各写各的计数器 %ld 次, 唯一区别是内存布局:\n", N_THREADS, (long)N_ADDS);
    double tp = run<Packed>("紧挨着(共享缓存行)");
    double td = run<Padded>("alignas(64)隔离  ");
    printf("加速 %.2fx\n", tp / td);
    // 原理: 写操作使整个 64B 缓存行在核间反复失效重拉(MESI 的 RFO),
    // 数据无关但缓存行在打架 → Packed 版慢; Padded 版每线程独占缓存行, 无互踩
    return 0;
}

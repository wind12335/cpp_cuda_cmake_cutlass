// 22 atomic vs mutex —— md 名词 ↔ 代码实体对照:
//   "裸 int++ 是读-改-写三步, 交错丢更新" → 第 1 段: bad 变量, 期望 800000 实际更少
//   "atomic 的 RMW(读-改-写一条龙)"      → 第 2 段: fetch_add, 期望=实际
//   "atomic 管不了组合一致性"            → 第 3 段: x/y 各自原子, 但"x==y"这个关系
//                                           必须靠 mutex 临界区保住
//   "读者也必须拿锁"                     → 第 3 段的观察者线程(不拿锁就会撞进中间态)
// 注意: 本文件建议 -O0 编译运行(命令见 md)。高优化级别下编译器/硬件可能让丢更新"碰巧不出现",
// 这恰恰说明数据竞争是概率性 bug —— 今天没炸不代表没竞争。
// 运行: g++ -std=c++17 -pthread 22_atomic_vs_mutex.cpp -o t22 && ./t22
#include <cstdio>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

constexpr int N_THREADS = 8;
constexpr int N_ADDS    = 100000;

int main() {
    // ---- 1. 裸 int++: 读-改-写三步, 线程交错丢更新 ----
    int bad = 0;
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i)
            ts.emplace_back([&bad] {
                for (int k = 0; k < N_ADDS; ++k) {
                    ++bad;
                    // 阻止编译器把 ++bad 缓存进寄存器(否则竞争窗口变小, 高优化级别下可能"碰巧不丢")
                    asm volatile("" ::: "memory");
                }
            });
        for (auto& t : ts) t.join();
    }
    printf("裸 int++:        期望 %d, 实际 %d (丢了 %d 次)\n",
           N_THREADS * N_ADDS, bad, N_THREADS * N_ADDS - bad);

    // ---- 2. atomic<int>++: 单条原子 RMW, 不丢 ----
    std::atomic<int> good{0};
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i)
            ts.emplace_back([&good] { for (int k = 0; k < N_ADDS; ++k) good.fetch_add(1, std::memory_order_relaxed); });
        for (auto& t : ts) t.join();
    }
    printf("atomic fetch_add: 期望 %d, 实际 %d ✓\n", N_THREADS * N_ADDS, good.load());

    // ---- 3. atomic 不够用的场景: 两个变量要保持一致 ----
    // 错误思路: 两个 atomic 各自正确, 但 (x==y) 的不变量会被交错破坏
    // 正确思路: mutex 临界区一次改两个
    std::atomic<int> x{0}, y{0};
    std::mutex m;
    std::atomic<bool> inconsistent{false};
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i)
            ts.emplace_back([&] {
                for (int k = 0; k < 1000; ++k) {
                    std::lock_guard<std::mutex> lk(m);
                    x.fetch_add(1); y.fetch_add(1);   // 临界区内: x,y 一起变
                }
            });
        ts.emplace_back([&] {
            for (int k = 0; k < 100000; ++k) {
                std::lock_guard<std::mutex> lk(m);    // 读者也必须拿锁!
                int a = x.load(), b = y.load();
                if (a != b) inconsistent = true;      // 拿锁读: 永远一致
            }
        });
        for (auto& t : ts) t.join();
    }
    printf("mutex 保一致性(读者也拿锁): x=%d y=%d, 观察到不一致=%s\n",
           x.load(), y.load(), inconsistent.load() ? "是" : "否");
    // 注意: 若读者不拿锁、直接裸读两个 atomic, 完全可能撞进"x 已加、y 未加"的中间态 ——
    // 那不是 mutex 失效, 而是读者绕过了临界区。这正是"组合一致性必须整段加锁"的原因。
    return 0;
}

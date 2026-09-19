// C09 实验: 线程/mutex/atomic/条件变量/async (全部 §C09 各小节的实测演示)
// 运行: g++ -std=c++17 -O2 -pthread C09_threads.cpp -o c09 && ./c09
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>
#include <vector>
#include <functional>

std::mutex m;
std::atomic<long> atomic_cnt{0};
long plain_cnt = 0;

int main() {
    // ---- §C09.0 看见多线程: 打印线程编号 ----
    std::thread t0([]{
        printf("① [线程%zu] 我是 t0\n",
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    });
    t0.join();
    printf("① [主线程%zu] 主线程编号\n",
           std::hash<std::thread::id>{}(std::this_thread::get_id()));

    // ---- §C09.3 裸写 vs atomic: 谁会丢更新 ----
    auto race = [&] { for (int i = 0; i < 100000; ++i) ++plain_cnt; };
    auto safe = [&] { for (int i = 0; i < 100000; ++i) atomic_cnt.fetch_add(1); };
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) ts.emplace_back(race);
    for (auto& t : ts) t.join();
    printf("② 4线程裸 ++: 期望 400000, 实际 %ld (%s)\n", plain_cnt,
           plain_cnt == 400000 ? "碰巧没丢(竞争是概率性的!)" : "丢更新了!");
    ts.clear();
    for (int i = 0; i < 4; ++i) ts.emplace_back(safe);
    for (auto& t : ts) t.join();
    printf("② 4线程 atomic: 期望 400000, 实际 %ld\n", atomic_cnt.load());

    // ---- §C09.3.2 atomic 读法两种 + fetch 家族 ----
    std::atomic<int> x{100};
    int old1 = x.fetch_sub(30);
    int old2 = x.exchange(7);
    int expected = 7;
    bool cas_ok = x.compare_exchange_strong(expected, 99);   // CAS: 是7就换99
    long via_load = atomic_cnt.load(), via_assign = atomic_cnt;
    printf("③ fetch家族: sub旧值=%d, exchange旧值=%d, CAS=%s, x=%d | 两种读法 %ld==%ld\n",
           old1, old2, cas_ok ? "成功" : "失败", x.load(), via_load, via_assign);

    // ---- §C09.4 cv 握手时序铁证: 生产者能在消费者睡觉时锁门 ----
    std::condition_variable cv;
    bool ready = false;
    std::thread consumer([&]{
        printf("④ [消费者] 拿锁, 进 wait(谓词 ready)...\n");
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]{ return ready; });        // 谓词不满足: 【放锁睡觉】
        printf("④ [消费者] 醒了! 重新拿到锁, 谓词成立, 干活\n");
    });                                           // lk 析构: 自动解锁
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("④ [生产者] 消费者正在 wait 里睡觉 —— 我现在锁门:\n");
    {
        std::lock_guard<std::mutex> lk(m);        // ← 能锁上 = wait 睡觉时锁是放的!
        printf("④ [生产者] 锁到了!(铁证) 改 ready=true\n");
        ready = true;
    }                                             // ← 解锁发生在这一行(作用域结束)
    printf("④ [生产者] notify_one: 只叫人, 不碰锁\n");
    cv.notify_one();
    consumer.join();

    // ---- §C09.5 async: 线程函数能返回值! 零 mutex 汇总 ----
    auto futA = std::async(std::launch::async, []{ return 6 * 7; });
    auto futB = std::async(std::launch::async, []{ return 100; });
    printf("⑤ async 两线程返回值汇总: %d (thread 做不到返回值)\n",
           futA.get() + futB.get());
    return 0;
}
// §C09.1.1 的"不 join 就崩"没法放进同一程序(会 terminate)——单独复现:
//   main(){ std::thread t([]{}); }   // 编译运行 → terminate called (退出码134)

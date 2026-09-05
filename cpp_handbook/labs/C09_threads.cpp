// C09 实验: 线程/mutex/atomic/条件变量/async
// 运行: g++ -std=c++17 -O2 -pthread C09_threads.cpp -o c09 && ./c09
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>
#include <vector>

std::mutex m;
std::atomic<long> atomic_cnt{0};
long plain_cnt = 0;

int main() {
    // ---- 1. 裸写 vs atomic vs mutex: 谁会丢更新 ----
    auto race = [&] { for (int i = 0; i < 100000; ++i) ++plain_cnt; };
    auto safe = [&] { for (int i = 0; i < 100000; ++i) atomic_cnt.fetch_add(1); };
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) ts.emplace_back(race);
    for (auto& t : ts) t.join();
    printf("4 线程裸 ++ : 期望 400000, 实际 %ld (%s)\n", plain_cnt,
           plain_cnt == 400000 ? "这次碰巧没丢(数据竞争是概率性的)" : "丢更新了!");
    ts.clear();
    for (int i = 0; i < 4; ++i) ts.emplace_back(safe);
    for (auto& t : ts) t.join();
    printf("4 线程 atomic: 期望 400000, 实际 %ld ✓\n", atomic_cnt.load());

    // ---- 2. mutex + lock_guard ----
    long guarded = 0;
    ts.clear();
    for (int i = 0; i < 4; ++i)
        ts.emplace_back([&] {
            for (int i = 0; i < 100000; ++i) {
                std::lock_guard<std::mutex> lk(m);   // RAII 加锁(前置 C06)
                ++guarded;
            }
        });
    for (auto& t : ts) t.join();
    printf("4 线程 mutex : 期望 400000, 实际 %ld ✓\n", guarded);

    // ---- 3. async: 异步任务取结果 ----
    auto fut = std::async(std::launch::async, [] {
        long s = 0;
        for (int i = 0; i < 1000000; ++i) s += i;
        return s;
    });
    printf("主线程先干点别的... 然后 async 的结果 = %ld\n", fut.get());

    // ---- 4. 生产者消费者(条件变量) ----
    std::condition_variable cv;
    bool ready = false;
    std::thread consumer([&] {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return ready; });      // 谓词不满足就睡
        printf("消费者被叫醒: ready=true\n");
    });
    {
        std::lock_guard<std::mutex> lk(m);
        ready = true;
    }
    cv.notify_one();
    consumer.join();
    return 0;
}

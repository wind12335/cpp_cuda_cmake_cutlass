// 24 手写生产者消费者 (mutex + condition_variable, 有界队列 + 关闭协议)
// 运行: g++ -std=c++17 -pthread 24_producer_consumer.cpp -o t24 && ./t24
#include <cstdio>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t cap) : cap_(cap) {}

    void push(T v) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });  // 谓词循环: 队满就睡
        if (closed_) return;                                              // 关闭后丢弃
        q_.push(std::move(v));
        not_empty_.notify_one();
    }

    bool pop(T& out) {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });      // 队空就睡
        if (q_.empty()) return false;                                     // closed 且排空 → 收工
        out = std::move(q_.front());
        q_.pop();
        not_full_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lk(m_);
        closed_ = true;
        not_empty_.notify_all();   // 状态变化影响所有等待者 → notify_all
        not_full_.notify_all();
    }

private:
    std::queue<T> q_;
    size_t cap_;
    std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    bool closed_ = false;
};

int main() {
    BoundedQueue<int> q(4);                       // 有界: 容量 4
    std::atomic<long> consumed_sum{0};            // 多线程累加必须原子 (否则本身丢更新)
    constexpr int P = 2, C = 3, N = 1000;

    std::vector<std::thread> pool;
    for (int p = 0; p < P; ++p)
        pool.emplace_back([&q, p] {
            for (int i = 0; i < N; ++i) q.push(p * N + i);
        });
    for (int c = 0; c < C; ++c)
        pool.emplace_back([&q, &consumed_sum] {
            int v;
            while (q.pop(v)) consumed_sum.fetch_add(v);   // pop 返回 false = closed 且排空
        });

    for (int p = 0; p < P; ++p) pool[p].join();   // 先等所有生产者完工
    q.close();                                    // 再关队列, 唤醒所有消费者
    for (int c = P; c < P + C; ++c) pool[c].join();

    long expect = 0;
    for (int p = 0; p < P; ++p)
        for (int i = 0; i < N; ++i) expect += p * N + i;
    printf("consumed sum = %ld, expect = %ld %s\n",
           consumed_sum.load(), expect, consumed_sum.load() == expect ? "✓ 无丢无重" : "✗ 有问题!");
    return 0;
}

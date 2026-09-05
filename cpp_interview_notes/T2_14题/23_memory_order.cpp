// 23 内存序: release 发布 / acquire 消费 —— md 名词 ↔ 代码实体对照:
//   "先装货再举旗"     → producer 里 payload 三次赋值(装货) + flag.store(release)(举旗)
//   "见旗才取货"       → consumer 里 flag.load(acquire) 之后的 payload 读取(取货)
//   "relaxed 只数数"   → consumer 自旋期间的 counter.fetch_add(relaxed)
//   "去掉同步对会怎样"  → 若 flag 改用 relaxed, 编译器/CPU 可把 payload 写入重排到举旗之后,
//                        消费者可能读到半成品 —— 文件尾注释有完整说明
// 运行: g++ -std=c++17 -pthread 23_memory_order.cpp -o t23 && ./t23
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

struct Payload { int a, b, c; };

std::atomic<bool>  flag{false};
Payload            payload{};   // 普通变量: 靠 flag 的同步对保证可见性
std::atomic<int>   counter{0};  // relaxed 计数示例

void producer() {
    payload.a = 1; payload.b = 2; payload.c = 3;              // ① 先填数据
    flag.store(true, std::memory_order_release);              // ② 再举旗: ① 的所有写入对 acquire 端可见
}

void consumer() {
    while (!flag.load(std::memory_order_acquire)) {           // ③ 看旗: acquire 保证之后的读不会越过它
        counter.fetch_add(1, std::memory_order_relaxed);      // 等待期间纯计数: relaxed 就够
        std::this_thread::yield();
    }
    // 到这里, release/acquire 同步对保证: payload 三个字段都是生产者写完后的值
    printf("consumer 读到 payload = {%d, %d, %d} (不可能是半成品)\n",
           payload.a, payload.b, payload.c);
}

int main() {
    std::thread p(producer), c(consumer);
    p.join(); c.join();
    printf("relaxed 计数(仅统计, 不参与逻辑): 自旋约 %d 次\n", counter.load());

    // 反例说明(不演示): 若 flag 用 relaxed 写/读, CPU/编译器可把 payload 写入重排到举旗之后,
    // 消费者就可能读到 {0,0,0} 的半成品 —— 这就是 acquire/release 存在的意义
    return 0;
}

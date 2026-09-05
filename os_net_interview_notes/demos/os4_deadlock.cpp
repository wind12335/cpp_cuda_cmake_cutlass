// os4_deadlock.cpp — 死锁四条件: 造一个(子进程)、诊断它、杀掉它、再用正确姿势避免
// 编译: g++ -O2 -pthread os4_deadlock.cpp -o os4 && ./os4
#include <cstdio>
#include <chrono>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

std::mutex A, B;   // 父子进程各有一份独立副本 (fork 后地址空间独立)

int main() {
    printf("=== 第 1 幕: 子进程里制造死锁 (T1: A→B, T2: B→A) ===\n");
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程: 两线程加锁顺序相反 → 循环等待
        std::thread t1([] {
            std::lock_guard<std::mutex> la(A);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 让对方先拿到另一把
            std::lock_guard<std::mutex> lb(B);
            puts("  T1 拿到 A+B (永远不会打印)");
        });
        std::thread t2([] {
            std::lock_guard<std::mutex> lb(B);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> la(A);
            puts("  T2 拿到 B+A (永远不会打印)");
        });
        t1.join(); t2.join();
        _exit(0);
    }
    // 父进程: 观察诊断
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == 0) {
        printf("  800ms 后子进程仍未退出 → 死锁确诊 (两线程互相等对方手里的锁)\n");
        printf("  现场诊断: gdb -p <pid> 后 `thread apply all bt`, 两个栈顶都停在 __lll_lock_wait;\n");
        printf("            或 cat /proc/<pid>/task/*/stack 对比。死锁进程无法自救, 只能杀:\n");
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        printf("  已 kill 子进程。教训: 生产代码用 scoped_lock/固定顺序预防, 而不是事后救。\n");
    } else {
        printf("  (子进程意外提前退出, 未复现死锁, 重跑一次)\n");
    }

    printf("\n=== 第 2 幕: 正确姿势 (同一对锁, 顺序一致) ===\n");
    auto worker = [](int id) {
        std::lock_guard<std::mutex> la(A);
        std::lock_guard<std::mutex> lb(B);   // 全员 A→B, 无环
        printf("  T%d 按序拿到 A→B, 完成\n", id);
    };
    std::thread g1(worker, 1), g2(worker, 2);
    g1.join(); g2.join();

    printf("=== 第 3 幕: std::scoped_lock 一次锁多把 (C++17, 内部免死锁算法) ===\n");
    auto worker2 = [](int id) {
        std::scoped_lock lk(A, B);
        printf("  T%d scoped_lock 完成\n", id);
    };
    std::thread g3(worker2, 3), g4(worker2, 4);
    g3.join(); g4.join();

    printf("\n四条件: 互斥、占有且等待、不可抢占、循环等待 —— 破任一条即无死锁。\n");
    printf("GPU 联动: 多流+event 的依赖成环、NCCL 各 rank collective 顺序不一致, 都是同款循环等待。\n");
    return 0;
}

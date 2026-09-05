// os1_threads_forks.c — 进程/线程/协程: 创建成本与切换成本实测
// 编译: g++ -O2 -pthread os1_threads_forks.c -o os1 && ./os1
#include <cstdio>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    constexpr int N = 1000;

    // ---- 1. 线程创建成本: pthread_create + join ----
    double t0 = now_ms();
    for (int i = 0; i < N; ++i) {
        std::thread t([]{});
        t.join();
    }
    double t1 = now_ms();
    printf("线程 创建+销毁:    %6.1f us/次  (共享地址空间, 只建栈+调度对象)\n", (t1 - t0) * 1000 / N);

    // ---- 2. 进程创建成本: fork + wait ----
    t0 = now_ms();
    for (int i = 0; i < N; ++i) {
        pid_t pid = fork();
        if (pid == 0) _exit(0);          // 子进程立刻退出
        waitpid(pid, nullptr, 0);
    }
    t1 = now_ms();
    printf("进程 fork+wait:    %6.1f us/次  (要复制页表/COW 设置, 贵好几倍)\n", (t1 - t0) * 1000 / N);

    // ---- 3. 上下文切换成本: 两线程用条件变量乒乓 20 万次 ----
    // 每次乒乓 = 2 次切换; 用 sched_yield 版本亦可对比
    int turn = 0;
    std::thread a([&] { for (int i = 0; i < 100000; ++i) { while (turn != 0) std::this_thread::yield(); turn = 1; } });
    std::thread b([&] { for (int i = 0; i < 100000; ++i) { while (turn != 1) std::this_thread::yield(); turn = 0; } });
    t0 = now_ms();
    a.join(); b.join();
    t1 = now_ms();
    printf("乒乓切换(yield):   %6.1f ns/次 (20万次乒乓, 含自旋) ← 真切换通常 1-5us, CPU 间更贵\n", (t1 - t0) * 1e6 / 200000);

    // ---- 结论口径 ----
    printf("\n面试口径: 线程=共享内存的执行流(创建us级, 切换us级), 进程=隔离地址空间(fork贵, 切换要刷TLB),\n");
    printf("          协程=用户态栈切换(几十ns, 不进内核)。GPU 语境: CUDA host 端线程池选线程不选 fork,\n");
    printf("          因为要共享 CUDA context。\n");
    return 0;
}

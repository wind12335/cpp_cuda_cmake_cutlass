// os5_shm_ipc.c — 进程间通信: 共享内存 vs 管道 带宽实测 (NCCL 单机通信同款机制)
// 编译: g++ -O2 -pthread os5_shm_ipc.c -o os5 && ./os5   (glibc 2.34+ 无需 -lrt)
#include <cstdio>
#include <cstring>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    const size_t TOTAL = 512UL << 20;   // 512MB
    const size_t CHUNK = 1UL << 20;     // 1MB 一批
    double t0, t1;

    // ---- 1. 匿名管道 (fork 父子进程, 单向搬运 512MB) ----
    int fd[2];
    if (pipe(fd)) return 1;
    t0 = now_ms();
    pid_t pid = fork();
    if (pid == 0) {
        close(fd[1]);
        char buf[CHUNK];
        size_t got = 0;
        while (got < TOTAL) {
            ssize_t n = read(fd[0], buf, CHUNK);   // ← 每次都要陷入内核: 数据 内核→用户
            if (n <= 0) break;
            got += (size_t)n;
        }
        _exit(0);
    }
    close(fd[0]);
    char* sendbuf = new char[CHUNK];
    memset(sendbuf, 7, CHUNK);
    for (size_t w = 0; w < TOTAL; w += CHUNK) {
        if (write(fd[1], sendbuf, CHUNK) < 0) { perror("write"); _exit(1); }
    }
    close(fd[1]);
    waitpid(pid, nullptr, 0);
    t1 = now_ms();
    printf("管道 pipe:        %6.1f ms  (%6.1f GB/s)  两次拷贝: 用户→内核→用户\n",
           t1 - t0, TOTAL / (t1 - t0) / 1e6);

    // ---- 2. 共享内存 (MAP_SHARED | MAP_ANONYMOUS, 父写子读, 零拷贝零内核往返) ----
    // 协议: 父写 chunk → 放旗标 → 子读 → 清旗标 → 父写下一批 (原子旗标, 简单 spin)
    float* shm = (float*)mmap(nullptr, CHUNK, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    std::atomic<int>* flag = (std::atomic<int>*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    flag->store(0);
    t0 = now_ms();
    pid_t pid2 = fork();
    if (pid2 == 0) {
        for (size_t r = 0; r < TOTAL; r += CHUNK) {
            while (flag->load(std::memory_order_acquire) == 0) sched_yield();  // 等数据就绪
            volatile float sink = shm[0]; (void)sink;                          // "读"这块共享内存
            flag->store(0, std::memory_order_release);                         // 清旗标: 可以写下一批
        }
        _exit(0);
    }
    for (size_t w = 0; w < TOTAL; w += CHUNK) {
        memset(shm, 7, CHUNK);                                  // 直接写共享页, 无拷贝无系统调用
        flag->store(1, std::memory_order_release);
        while (flag->load(std::memory_order_acquire) != 0) sched_yield();
    }
    waitpid(pid2, nullptr, 0);
    t1 = now_ms();
    printf("共享内存 shm:     %6.1f ms  (%6.1f GB/s)  零拷贝: 两进程映射同一批物理页\n",
           t1 - t0, TOTAL / (t1 - t0) / 1e6);

    // ---- 口径 ----
    printf("\nIPC 谱系: 管道/消息队列(内核中转,两次拷贝) < 共享内存(零拷贝,需自己同步) < 同进程内存。\n");
    printf("NCCL 单机同框: P2P 走 NVLink 直达; 不支持 P2P 的组合退化为经 host 的共享内存(SHM transport)\n");
    printf("—— 上面这套机制就是它的底座。socket(本机回环)还要多走协议栈, 最慢。\n");
    delete[] sendbuf; munmap(shm, CHUNK); munmap((void*)flag, 4096);
    return 0;
}

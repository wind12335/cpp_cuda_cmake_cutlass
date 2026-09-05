// os6_syscall_vdso.c — 用户态/内核态: 真系统调用 vs vDSO 的成本差
// 编译: g++ -O2 -pthread os6_syscall_vdso.c -o os6 && ./os6
#include <cstdio>
#include <chrono>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    constexpr long N = 10'000'000;
    struct timespec dummy;

    // ---- 1. vDSO: clock_gettime 不进内核 (内核把"只读时间页"映射进用户空间) ----
    double t0 = now_ms();
    for (long i = 0; i < N; ++i) clock_gettime(CLOCK_MONOTONIC, &dummy);
    double t1 = now_ms();
    printf("clock_gettime (vDSO):   %6.1f ns/次  ← 纯用户态读内存, 没有模式切换\n", (t1 - t0) * 1e6 / N);

    // ---- 2. 真系统调用: getpid 强制走 syscall 指令 (旧 glibc 会缓存, 这里直接 syscall) ----
    t0 = now_ms();
    for (long i = 0; i < N; ++i) syscall(SYS_getpid);
    t1 = now_ms();
    printf("syscall(getpid):        %6.1f ns/次  ← 陷入内核: 保存现场→查系统调用表→恢复现场\n", (t1 - t0) * 1e6 / N);

    // ---- 3. 稍贵的系统调用做对照 ----
    t0 = now_ms();
    for (long i = 0; i < N / 100; ++i) getppid();
    t1 = now_ms();
    printf("getppid (经 glibc):     %6.1f ns/次  (量级参考)\n", (t1 - t0) * 1e6 / (N / 100));

    printf("\n面试口径: 模式切换 = 用户态→内核态(保存上下文+权限切换), 量级 ~100-300ns, 是普通函数调用的百倍;\n");
    printf("          所以有 vDSO(把只读数据/无危险操作映射进用户态)、批处理(write 合并)、零拷贝(减少调用次数);\n");
    printf("          epoll 一次 wait 收一批事件, 正是'摊薄陷入次数'的设计(联动 NET-03)。\n");
    return 0;
}

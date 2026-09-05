// os7_cache_levels.c — CPU 缓存层级: 用"工作集大小 vs 读取带宽"扫出 L1/L2/L3/内存 四级悬崖
// 编译: g++ -O2 -pthread os7_cache_levels.c -o os7 && ./os7
#include <cstdio>
#include <cstdlib>
#include <time.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static unsigned long long g_sink = 0;   // 防止读循环被编译器删掉

// 对 sz 字节做一遍"每 cache line 读 8 字节"的流式扫
static inline void one_pass(char* buf, size_t sz) {
    for (size_t i = 0; i < sz; i += 64)
        g_sink += ((unsigned long long*)(buf + i))[0];
}

int main() {
    // 工作集从小到大: 依次超出 L1(48KB 量级) → L2(1-2MB) → L3(24-32MB) → 内存
    const size_t sizes[] = {16UL << 10, 32UL << 10, 64UL << 10, 256UL << 10, 1UL << 20,
                            2UL << 20, 8UL << 20, 24UL << 20, 32UL << 20, 128UL << 20, 512UL << 20};
    const int NSIZE = (int)(sizeof(sizes) / sizeof(sizes[0]));
    char* buf = (char*)malloc(sizes[NSIZE - 1]);
    for (size_t i = 0; i < sizes[NSIZE - 1]; i += 4096) buf[i] = (char)i;   // 先触碰: 分配物理页

    printf("%10s %14s\n", "工作集", "读带宽");
    for (int s = 0; s < NSIZE; ++s) {
        size_t sz = sizes[s];
        one_pass(buf, sz);                       // 预热
        // 采样窗口 ~100ms: 数清跑了多少遍, 算平均带宽 (比单遍计时稳)
        int passes = 0;
        double t0 = now_ms(), t1;
        do {
            one_pass(buf, sz);
            ++passes;
            t1 = now_ms();
        } while (t1 - t0 < 100.0);
        double gbs = (double)sz * passes / ((t1 - t0) / 1000.0) / 1e9;
        printf("%8zu KB %10.1f GB/s  %s\n", sz >> 10, gbs,
               sz <= 32UL << 10 ? "← L1 装得下" :
               sz <= 2UL << 20  ? "← L2 级" :
               sz <= 32UL << 20 ? "← L3 级" : "← 掉到内存");
    }
    printf("(校验和 %llu, 防优化)\n", g_sink);
    free(buf);

    printf("\n读法: 带宽随工作集增大出现 3-4 级悬崖, 悬崖位置 = L1/L2/L3/DRAM 边界。\n");
    printf("面试口径: 延迟 L1 ~4 cycle / L2 ~12 / L3 ~40 / DRAM ~200+; MESI 保证核间'看到同一个值',\n");
    printf("          但不保证顺序(内存序的事), false sharing 是它的副作用 —— 均联动 C++ 23/25 题。\n");
    return 0;
}

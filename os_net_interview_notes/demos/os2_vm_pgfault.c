// os2_vm_pgfault.c — 虚拟内存 / 页表 / 缺页中断: 看得见 + 摸得着
// 编译: g++ -O2 -pthread os2_vm_pgfault.c -o os2 && ./os2
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// 从 /proc/self/stat 读 minor fault 计数: pid (comm) 之后第 8 个字段是 min_flt
// (字段序: state ppid pgrp session tty tpgid flags minflt)
static long min_faults() {
    FILE* f = fopen("/proc/self/stat", "r");
    if (!f) return -1;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    char* p = strrchr(buf, ')');
    long minflt = 0;
    sscanf(p + 2, " %*c %*d %*d %*d %*d %*d %*d %ld", &minflt);
    return minflt;
}

int main() {
    // ---- 1. 虚拟内存"看得见": /proc/self/maps 的分区 ----
    printf("=== 地址空间布局 (虚拟地址, 每进程独立一套) ===\n");
    FILE* f = fopen("/proc/self/maps", "r");
    char line[512];
    char stack_line[512] = "", heap_line[512] = "", libc_line[512] = "";
    while (fgets(line, sizeof(line), f)) {
        if (!stack_line[0] && strstr(line, "[stack]")) strcpy(stack_line, line);
        else if (!heap_line[0] && strstr(line, "[heap]")) strcpy(heap_line, line);
        else if (!libc_line[0] && strstr(line, "libc.so")) strcpy(libc_line, line);
    }
    fclose(f);
    printf("  %s  %s  %s", stack_line, heap_line, libc_line);

    // ---- 2. 缺页中断"摸得着": mmap 只给虚拟地址, 第一次摸才分配物理页 ----
    const size_t SZ = 256UL << 20;   // 256MB
    printf("\n=== 256MB mmap: 分配虚拟地址(立即返回) vs 首次触碰(缺页分配物理页) ===\n");
    double t0 = now_ms();
    char* p = (char*)mmap(nullptr, SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    double t1 = now_ms();
    printf("mmap 本身:        %.3f ms  (只登记 VMA[内核里记录\"这段虚拟地址归谁\"的账本], 0 物理页分配)\n", t1 - t0);

    long f0 = min_faults();
    t0 = now_ms();
    memset(p, 1, SZ);                 // 首次触碰: 每页(4KB)一次 minor fault + 分配零页
    t1 = now_ms();
    long f1 = min_faults();
    printf("首次 memset:      %.1f ms  (慢: %ld 次缺页, 每页一拍内核开销)\n", t1 - t0, f1 - f0);

    t0 = now_ms();
    memset(p, 2, SZ);                 // 第二次: 页表已热, 纯内存带宽
    t1 = now_ms();
    printf("第二次 memset:    %.1f ms  (快: 无缺页, 纯带宽, 已写入的页在内存里)\n", t1 - t0);
    munmap(p, SZ);

    // ---- 面试口径 ----
    printf("\n面试口径: mmap 后没碰就几乎不占物理内存(虚拟≠物理); TLB 缓存页表项, 进程切换换页表 → TLB 刷/切换成本;\n");
    printf("          缺页分 minor(物理页分配/换入) 和 major(磁盘换入, 差千倍)。\n");
    printf("          GPU 联动: cudaMalloc 也是「登记 + 首次触碰物理页」的思路, 统一内存(UM)的按需迁移就是 major fault 思路。\n");
    return 0;
}

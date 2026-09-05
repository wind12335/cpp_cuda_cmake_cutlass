// net5_bandwidth.cu — 链路带宽量级: 能测的现场测, 测不了的给公认量级, 一张表装进脑子
// 编译: nvcc -std=c++17 -O2 -arch=sm_89 net5_bandwidth.cu -o net5 && ./net5
#include <cstdio>
#include <cuda_runtime.h>
#include <time.h>

static inline double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    printf("=== 本机实测 ===\n");
    // ---- 1. Host 内存带宽 (大块 memcpy, 单线程; 先预触碰两块缓冲, 排除缺页干扰) ----
    const size_t SZ = 1UL << 30;   // 1GB
    char *a = (char*)malloc(SZ), *b = (char*)malloc(SZ);
    memset(a, 1, SZ);
    memset(b, 0, SZ);
    double best = 1e18;
    for (int r = 0; r < 5; ++r) {
        double t0 = now_ms();
        memcpy(b, a, SZ);
        double t1 = now_ms();
        double gbs = 2.0 * SZ / ((t1 - t0) / 1000.0) / 1e9;   // 读+写都算流量
        if (gbs < best) best = gbs;
    }
    printf("Host DRAM memcpy(读+写): %6.1f GB/s  (WSL2 虚拟化下偏低, 裸机常见 20-60)\n", best);

    // ---- 2. PCIe H2D / D2H (pinned) ----
    char *h_pin, *d;
    cudaMallocHost(&h_pin, SZ);
    cudaMalloc(&d, SZ);
    memset(h_pin, 1, SZ);
    best = 1e18;
    for (int r = 0; r < 5; ++r) {
        double t0 = now_ms();
        cudaMemcpy(d, h_pin, SZ, cudaMemcpyHostToDevice);
        double t1 = now_ms();
        double gbs = SZ / ((t1 - t0) / 1000.0) / 1e9;
        if (gbs < best) best = gbs;
    }
    printf("PCIe H2D (pinned):       %6.1f GB/s   (4060 Laptop = PCIe4 x8 理论 ~16 GB/s)\n", best);
    best = 1e18;
    for (int r = 0; r < 5; ++r) {
        double t0 = now_ms();
        cudaMemcpy(h_pin, d, SZ, cudaMemcpyDeviceToHost);
        double t1 = now_ms();
        double gbs = SZ / ((t1 - t0) / 1000.0) / 1e9;
        if (gbs < best) best = gbs;
    }
    printf("PCIe D2H (pinned):       %6.1f GB/s\n", best);

    // ---- 3. 量级表 (公认数字, 面试要张口就来) ----
    printf("\n=== 全链路量级表 (面试背诵版) ===\n");
    printf("%-28s %12s  备注\n", "链路", "带宽");
    printf("%-28s %12s  --------------------------------\n", "", "");
    printf("%-28s %12s  NVMe 盘也比网络快\n", "NVMe SSD", "~7 GB/s");
    printf("%-28s %12s  本机回环 TCP 实测就是这个量级\n", "本机回环 TCP", "~5-20 GB/s");
    printf("%-28s %12s  服务器 DDR5 多通道聚合\n", "Host DRAM", "~50-100 GB/s");
    printf("%-28s %12s  Gen4 x16=32 / Gen5 x16=64\n", "PCIe 4.0/5.0 x16", "32/64 GB/s");
    printf("%-28s %12s  400Gb/s 网卡 = 50GB/s, RDMA 绕内核\n", "IB/RoCE 400Gb", "50 GB/s");
    printf("%-28s %12s  H100 每卡双向 900GB/s\n", "NVLink 4.0", "900 GB/s");
    printf("%-28s %12s  8 卡全互联交换, AllReduce 的物理基础\n", "NVSwitch", "~1.6+ TB/s");
    printf("%-28s %12s  24GB/s / 1TB/s / 3.35TB/s\n", "显存(GDDR6/GDDR6X/HBM3e)", "逐级跳");

    printf("\n面试口径: 这张表决定 NCCL 的一切决策 —— 单机优先 NVLink(P2P), 不行退 SHM, 跨机走 RDMA;\n");
    printf("  通信能不能被计算藏住, 就比'计算时间 vs 数据量÷链路带宽' —— 你 overlap 课题的 cost model 底表。\n");
    free(a); free(b); cudaFreeHost(h_pin); cudaFree(d);
    return 0;
}

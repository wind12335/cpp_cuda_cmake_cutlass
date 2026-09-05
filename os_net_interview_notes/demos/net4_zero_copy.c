// net4_zero_copy.c — 零拷贝: read+write vs mmap+write vs sendfile 实测
// 编译: g++ -O2 -pthread net4_zero_copy.c -o net4 && ./net4
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

using clk = std::chrono::steady_clock;
static double ms_since(clk::time_point t) {
    return std::chrono::duration<double, std::milli>(clk::now() - t).count();
}

int main() {
    const size_t SZ = 256UL << 20;   // 256MB
    const char* src_path = "/dev/shm/net4_src";
    const char* dst_path = "/dev/shm/net4_dst";

    // 准备源文件 (在 /dev/shm = 内存盘, 排除磁盘噪声, 只看 CPU/拷贝路径差异)
    int src = open(src_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (ftruncate(src, SZ)) return 1;
    char* init = (char*)mmap(nullptr, SZ, PROT_READ | PROT_WRITE, MAP_SHARED, src, 0);
    memset(init, 7, SZ);
    munmap(init, SZ);

    // ---------- 方法 A: read + write (传统: 4 次拷贝, 4 次模式切换) ----------
    char* heap = (char*)malloc(1 << 20);
    auto t0 = clk::now();
    {
        int dst = open(dst_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        ssize_t n;
        lseek(src, 0, SEEK_SET);
        while ((n = read(src, heap, 1 << 20)) > 0)      // 拷贝1: 磁盘(DMA)→页缓存  拷贝2: 内核→用户
            write(dst, heap, n);                         // 拷贝3: 用户→内核      拷贝4: 内核→磁盘(DMA)
        close(dst);
    }
    printf("read + write : %6.1f ms  (数据过 CPU 两次, 陷入内核 512 次)\n", ms_since(t0));

    // ---------- 方法 B: mmap + write (省"内核→用户"拷贝) ----------
    t0 = clk::now();
    {
        int dst = open(dst_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        char* m = (char*)mmap(nullptr, SZ, PROT_READ, MAP_SHARED, src, 0);
        for (size_t off = 0; off < SZ; off += (1 << 20)) write(dst, m + off, 1 << 20);
        munmap(m, SZ);
        close(dst);
    }
    printf("mmap + write : %6.1f ms  (少一次拷贝: 用户态直接读页缓存)\n", ms_since(t0));

    // ---------- 方法 C: sendfile (全程内核态, 零用户态拷贝) ----------
    // 经典场景是 文件→socket, 这里配一对 socketpair + 排水线程模拟"发送给客户端"
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    long drained = 0;
    FILE* log = fopen("/dev/null", "w");
    (void)log;
    std::thread drainer([&] {
        char tmp[1 << 20];
        ssize_t n;
        while ((n = read(sv[0], tmp, sizeof(tmp))) > 0) drained += n;   // 只排水不消化
        printf("  (drain 线程共收 %ld MB)\n", drained >> 20);
    });
    t0 = clk::now();
    {
        lseek(src, 0, SEEK_SET);
        off_t off = 0;
        while (off < (off_t)SZ) {
            ssize_t n = sendfile(sv[1], src, &off, 1 << 20);   // 内核内 页缓存→socket, 数据不过 CPU 用户态
            if (n <= 0) break;
            off += n;
        }
        close(sv[1]);
    }
    drainer.join();
    printf("sendfile     : %6.1f ms  (零拷贝: 内核缓冲区之间直搬, CPU 零参与数据搬运)\n", ms_since(t0));
    (void)log;
    close(sv[0]); close(src);
    unlink(src_path); unlink(dst_path); free(heap);

    printf("\n面试口径: 传统 read+write = 4 次拷贝 2 次多余; mmap 省 1 次拷贝但页表/缺页有代价(小文件不划算);\n");
    printf("  sendfile 全程内核, 用户态只传文件描述符 —— Kafka/Nginx 静态文件的看家本领;\n");
    printf("  GPU 同构: GPUDirect 把'内核缓冲区直搬'升级为'显存↔网卡直搬', 连 CPU 内存都不经过 —— NCCL 跨机的底座。\n");
    return 0;
}

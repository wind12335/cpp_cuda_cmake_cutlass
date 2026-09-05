// net2_tcp_vs_udp.c — TCP vs UDP: 本机回环吞吐 + 一次 RTT 延迟实测
// 编译: g++ -O2 -pthread net2_tcp_vs_udp.c -o net2 && ./net2
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

using clk = std::chrono::steady_clock;
static double ms_since(clk::time_point t0) {
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}

int main() {
    const size_t TOTAL = 256UL << 20;   // 256MB
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // ================= TCP: 吞吐 + RTT =================
    int lst = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_port = htons(9528);
    bind(lst, (sockaddr*)&addr, sizeof(addr));
    listen(lst, 1);
    std::thread tcp_srv([&] {
        int c = accept(lst, nullptr, nullptr);
        std::vector<char> buf(1 << 20);
        size_t got = 0;
        while (got < TOTAL) {
            ssize_t n = recv(c, buf.data(), buf.size(), 0);
            if (n <= 0) break;
            got += (size_t)n;
        }
        close(c);
    });
    int c = socket(AF_INET, SOCK_STREAM, 0);
    connect(c, (sockaddr*)&addr, sizeof(addr));
    std::vector<char> buf(1 << 20);
    memset(buf.data(), 1, buf.size());
    auto t0 = clk::now();
    for (size_t s = 0; s < TOTAL; s += buf.size()) send(c, buf.data(), buf.size(), 0);
    auto t1 = clk::now();
    tcp_srv.join();
    printf("TCP  256MB 回环吞吐: %6.1f GB/s\n", TOTAL / ms_since(t0) / 1e6);

    // RTT: 1 字节 ping-pong 1000 次
    int rtt_sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_port = htons(9529);
    int lst2 = socket(AF_INET, SOCK_STREAM, 0);
    bind(lst2, (sockaddr*)&addr, sizeof(addr));
    listen(lst2, 1);
    std::thread echo([&] {
        int e = accept(lst2, nullptr, nullptr);
        char b;
        for (int i = 0; i < 1000; ++i) { recv(e, &b, 1, 0); send(e, &b, 1, 0); }
        close(e);
    });
    connect(rtt_sock, (sockaddr*)&addr, sizeof(addr));
    char b = 0;
    t0 = clk::now();
    for (int i = 0; i < 1000; ++i) { send(rtt_sock, &b, 1, 0); recv(rtt_sock, &b, 1, 0); }
    t1 = clk::now();
    echo.join();
    printf("TCP  1字节 RTT:      %6.1f us  (1000 次乒乓取均值, 本机回环)\n", ms_since(t0));
    close(rtt_sock); close(lst2); close(lst);

    // ================= UDP: 数据报吞吐 =================
    int u = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_port = htons(9530);
    int ur = socket(AF_INET, SOCK_DGRAM, 0);
    int big = 8 << 20;
    setsockopt(ur, SOL_SOCKET, SO_RCVBUF, &big, sizeof(big));   // 加大接收缓冲, 减少丢包
    struct timeval tv{2, 0};                                    // 2 秒收不到就放弃: UDP 的包可能真丢了
    setsockopt(ur, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    bind(ur, (sockaddr*)&addr, sizeof(addr));
    const int PKTS = 100000;
    std::thread udp_srv([&] {
        std::vector<char> rb(65536);
        int got = 0;
        while (got < PKTS) {
            ssize_t n = recvfrom(ur, rb.data(), rb.size(), 0, nullptr, nullptr);
            if (n <= 0) break;   // 超时/错误: 有包永远来不了了
            got++;
        }
        printf("UDP  接收 %d/%d 包 (%.1f%%)%s\n", got, PKTS, got * 100.0 / PKTS,
               got < PKTS ? " ← 有丢包: UDP 不保证送达, 接收端只能超时止损" : "");
    });
    std::vector<char> pb(1024);   // 1KB 数据报
    memset(pb.data(), 2, pb.size());
    t0 = clk::now();
    for (int i = 0; i < PKTS; ++i) sendto(u, pb.data(), pb.size(), 0, (sockaddr*)&addr, sizeof(addr));
    t1 = clk::now();
    udp_srv.join();
    printf("UDP  1KB 数据报:     %6.1f 万包/s (发送侧)\n", PKTS / ms_since(t0) * 1000 / 1e4);

    printf("\n面试口径: TCP=可靠字节流(有序/重传/拥塞控制, 头部20B+), UDP=不可靠数据报(无连接, 头部8B, 快而裸);\n");
    printf("  选谁: 要可靠有序→TCP(HTTP/RPC); 要低延迟容忍丢失→UDP(视频/游戏/DNS);\n");
    printf("  RDMA/RoCE 则把可靠传输下沉到网卡硬件, 绕过内核协议栈 —— NCCL 跨机走的就是它(联动 NET-05)。\n");
    close(u); close(ur);
    return 0;
}

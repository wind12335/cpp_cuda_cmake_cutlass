// net1_tcp_timewait.c — TCP 握手挥手: 亲手造出 TIME_WAIT 并数它
// 编译: g++ -O2 -pthread net1_tcp_timewait.c -o net1 && ./net1
#include <cstdio>
#include <cstring>
#include <thread>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main() {
    // 服务端线程: accept 20 个连接, 各收一个字节回一个字节
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));   // 服务端必配: 避免 TIME_WAIT 卡住重启
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(9527);
    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, 16);

    std::thread server([&] {
        for (int i = 0; i < 20; ++i) {
            int c = accept(srv, nullptr, nullptr);
            char b;
            recv(c, &b, 1, 0);
            send(c, &b, 1, 0);
            close(c);                      // 服务端也关, 但谁先关谁 TIME_WAIT —— 客户端先关
        }
    });

    // 客户端: 20 次 "连接→发→收→先关"
    for (int i = 0; i < 20; ++i) {
        int c = socket(AF_INET, SOCK_STREAM, 0);
        connect(c, (sockaddr*)&addr, sizeof(addr));   // ← 三次握手在这一步内核完成
        char b = 'x';
        send(c, &b, 1, 0);
        recv(c, &b, 1, 0);
        close(c);                           // 客户端主动关 → 客户端侧进入 TIME_WAIT (60s~2min)
    }
    server.join();

    // 数一数刚才造出的 TIME_WAIT
    FILE* f = popen("ss -tan state time-wait 2>/dev/null | tail -n +2 | wc -l", "r");
    int n = -1;
    if (f) { char buf[64]; if (fgets(buf, sizeof(buf), f)) n = atoi(buf); pclose(f); }
    printf("主动关闭 20 次后, 本机 TIME_WAIT 数量: %d\n", n);
    printf("观察单条: ss -tan state time-wait | head   (每条会停留 60s~2min)\n\n");

    printf("面试口径: 三次握手=同步双方初始序号(SYN/ACK), 两次不够(旧的重复 SYN 会建幽灵连接);\n");
    printf("  四次挥手多一次是因为关闭是半双工的(收到 FIN 后我还能发完剩余数据);\n");
    printf("  TIME_WAIT 只在'主动关闭方': 等 2MSL 让网络中迷路的旧包自然消亡, 防止污染新连接。\n");
    printf("  副作用: 高频短连接会把端口耗尽(客户端端口有限), 解法=连接池/长连接/SO_REUSEADDR(服务端)。\n");
    close(srv);
    return 0;
}

// net3_epoll_lt_et.c — epoll 为什么快 + LT/ET 行为差异 (用 pipe 演示, 无需网络)
// 编译: g++ -O2 -pthread net3_epoll_lt_et.c -o net3 && ./net3
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

int main() {
    int fds[2];
    if (pipe(fds)) return 1;               // fds[0] 读端, fds[1] 写端
    // 读端必须非阻塞: "读到 EAGAIN" 是 ET 的收工信号; 阻塞 fd 读空会永久挂起(亲手踩过!)
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    int ep = epoll_create1(0);

    // ============ LT (水平触发, 默认): 只要"还有数据没读完", 每次 epoll_wait 都报告 ============
    printf("=== LT 模式: 写入 100 字节后, 连续 3 次 epoll_wait ===\n");
    epoll_event ev{};
    ev.events = EPOLLIN;                   // 默认 LT
    ev.data.fd = fds[0];
    epoll_ctl(ep, EPOLL_CTL_ADD, fds[0], &ev);
    char payload[100];
    memset(payload, 'A', sizeof(payload));
    write(fds[1], payload, sizeof(payload));

    for (int i = 0; i < 3; ++i) {
        epoll_event out{};
        int n = epoll_wait(ep, &out, 1, 100);   // 不读数据, 只是等
        printf("  第 %d 次 epoll_wait: 返回 %d (LT: 没读完就一直吵)\n", i + 1, n);
    }

    // ============ ET (边缘触发): 只在状态"变化"时报一次, 必须一口气抽干 ============
    printf("=== ET 模式: 同样的 100 字节, 重新注册后 ===\n");
    epoll_ctl(ep, EPOLL_CTL_DEL, fds[0], nullptr);
    // 先把旧数据抽干, 再重新注册为 ET
    char drain[4096];
    while (read(fds[0], drain, sizeof(drain)) > 0) {}
    ev.events = EPOLLIN | EPOLLET;         // 加 EPOLLET = 边缘触发
    epoll_ctl(ep, EPOLL_CTL_ADD, fds[0], &ev);

    write(fds[1], payload, sizeof(payload));
    for (int i = 0; i < 3; ++i) {
        epoll_event out{};
        int n = epoll_wait(ep, &out, 1, 100);
        printf("  第 %d 次 epoll_wait: 返回 %d%s\n", i + 1, n,
               n > 0 ? " (ET: 只报这一次)" : " (ET: 已报过, 沉默)");
    }
    // ET 的正确姿势: 报警后必须 read 到 EAGAIN —— 重新注册后再演示
    while (read(fds[0], drain, sizeof(drain)) > 0) {}
    write(fds[1], "CCCC...", 7);
    epoll_event out{};
    if (epoll_wait(ep, &out, 1, 100) > 0) {
        ssize_t total = 0, n;
        while ((n = read(fds[0], drain, sizeof(drain))) > 0) total += n;   // ← 读到 EAGAIN 才停
        printf("  正确姿势: 一次报警后 read 到 EAGAIN, 共读 %zd 字节\n", total);
        // 再等一次: 因为已抽干(状态没变化), ET 不再报警
        int n2 = epoll_wait(ep, &out, 1, 100);
        printf("  抽干后再等: 返回 %d (没新数据就是没新事件)\n", n2);
    }

    printf("\n面试口径: select/poll 每次 O(n) 拷贝全部 fd 进内核再线性扫描; epoll 用红黑树管 fd +\n");
    printf("  就绪链表回调, epoll_wait 只取就绪列表 → O(活跃数), 百万连接不慌 —— 这就是'快'的来源;\n");
    printf("  LT 好写(没读完会一直提醒), ET 高效(少唤醒)但必须配非阻塞 fd + 读到 EAGAIN, 否则饿死连接。\n");
    close(fds[0]); close(fds[1]); close(ep);
    return 0;
}

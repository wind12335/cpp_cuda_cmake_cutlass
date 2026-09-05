// C02 实验: 控制流/函数/传参/lambda
// 运行: g++ -std=c++17 -O2 C02_control_flow.cpp -o c02 && ./c02
#include <cstdio>
#include <vector>
#include <algorithm>

// §C02.4 三种传参对照
void byValue(int x)  { x = 100; }   // 副本
void byRef(int& x)   { x = 100; }   // 原件的别名
void byPtr(int* x)   { if (x) *x = 100; }

// §C02.5 重载 + 默认参数
void launch(const char* name, int blocks, int threads = 256) {
    printf("launch %s: blocks=%d threads=%d\n", name, blocks, threads);
}

int main() {
    // §C02.1 带初始化的 if (C++17) + auto
    if (int n = 42; n % 2 == 0) printf("42 是偶数 (n=%d 只在本 if 里可用)\n", n);
    int x = 42;
    if (auto m = x * 2; m > 50)                        // auto: 编译器推断 m 是 int
        printf("x*2=%d > 50 (m 只在本 if/else 里存在)\n", m);
    // printf("%d", m);                                // ✗ 打开注释: 编译错, m 已经不在了

    // §C02.1.3 无 default 的 switch: 没匹配就整段跳过
    int t = 5;
    switch (t % 3) {
        case 0: printf("余0\n"); break;
        case 1: printf("余1\n"); break;
    }                                                  // t%3==2 → 没有匹配分支, 什么都不发生
    printf("无 default 的 switch 走完了 (t%%3==2 时上面静默跳过)\n");

    // §C02.2 range-for
    std::vector<int> v = {1, 2, 3};
    for (int& e : v) e *= 10;                        // 用引用才能改到原件
    printf("翻十倍后: ");
    for (const int& e : v) printf("%d ", e);          // 只读用 const 引用
    printf("\n");

    // §C02.2.1 副本 vs 别名: 打印地址眼见为实
    std::vector<int> a2 = {10, 20};
    printf("v[0] 地址=%p\n", (void*)&a2[0]);
    for (int& e : a2) printf("  别名版(int&)  &e=%p  ← 和 v[i] 同一块!\n", (void*)&e);
    for (int e : a2)  printf("  副本版(int)   &e=%p  ← 每轮一个新地址(栈上的副本)\n", (void*)&e);

    // §C02.4 三种传参
    int a = 1;
    byValue(a); printf("byValue 后 a=%d (没变)\n", a);
    byRef(a);   printf("byRef   后 a=%d (变了)\n", a);
    byPtr(&a);  printf("byPtr   后 a=%d (变了)\n", a);

    // §C02.5 重载与默认参数
    launch("默认", 32);
    launch("显式", 32, 512);

    // §C02.7 lambda: 排序规则
    std::vector<int> s = {3, 1, 2};
    std::sort(s.begin(), s.end(), [](int x, int y) { return x > y; });  // 降序
    printf("lambda 降序排序: ");
    for (int e : s) printf("%d ", e);
    printf("\n");

    // 捕获演示: 值捕=快照, 引用捕=原件
    int base = 100;
    auto byVal = [base](int x) { return base + x; };
    auto byRefC = [&base](int x) { base += x; return base; };
    printf("值捕获加法=%d (base 仍=%d)\n", byVal(5), base);
    printf("引用捕获加法=%d (base 变=%d)\n", byRefC(5), base);
    return 0;
}

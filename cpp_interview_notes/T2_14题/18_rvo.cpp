// 18 RVO / NRVO
// 运行: g++ -std=c++17 18_rvo.cpp -o t18 && ./t18
// 对比: g++ -std=c++17 -fno-elide-constructors 18_rvo.cpp -o t18b && ./t18b
#include <cstdio>

struct W {
    W()  { puts("  构造"); }
    W(const W&)  { puts("  拷贝构造"); }
    W(W&&) noexcept { puts("  移动构造"); }
    ~W() = default;
};

W rvo()        { return W{}; }   // 返回纯临时: C++17 强制消除 → 只打印"构造"
W nrvo()       { W w; return w; }// 返回具名变量: NRVO, 编译器优化 → 通常也只打印"构造"
W branch(bool f) { W a, b; return f ? a : b; }  // 两个对象走分支: 消除不了 → 移动
W param(W p)   { return p; }     // 返回参数: 消除不了 → 移动

int main() {
    puts("--- RVO (return 临时) ---");          W x1 = rvo();
    puts("--- NRVO (return 具名) ---");         W x2 = nrvo();
    puts("--- 分支返回 (RVO 不可用) ---");       W x3 = branch(false);   // 期望: 一次移动
    puts("--- 参数返回 (NRVO 不可用) ---");      W x4 = param(W{});      // 期望: 一次移动
    // 对照实验: 用 -fno-elide-constructors 重编, rvo() 也会多出一次移动
    return 0;
}

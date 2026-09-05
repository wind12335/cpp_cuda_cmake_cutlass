// 10 移动构造什么时候被触发
// 运行: g++ -std=c++17 10_move_ctor_triggers.cpp -o t10 && ./t10
#include <cstdio>
#include <vector>
#include <utility>

class Tracked {
public:
    Tracked() { puts("  默认构造"); }
    Tracked(const Tracked&) { puts("  拷贝构造"); }
    Tracked(Tracked&&) noexcept { puts("  移动构造"); }   // 把 noexcept 删掉再跑, 看第 3 段的变化!
    Tracked& operator=(const Tracked&) { puts("  拷贝赋值"); return *this; }
    ~Tracked() = default;
};

Tracked makeValue() {          // 返回局部对象 → C++17 保证 RVO, 一次都不移动
    Tracked t;
    return t;                  // 不是 return std::move(t) —— 那样反而阻止 RVO!
}

Tracked makeBranch(bool flag) { // 两个不同对象走分支 → RVO 无法合并, 走"移动"或"拷贝"
    Tracked a, b;
    return flag ? a : b;       // 具名对象: NRVO 不可用 → 移动
}

void takeByValue(Tracked x) {} // 值传参

int main() {
    printf("--- ① 显式 std::move ---\n");
    Tracked t1; Tracked t2 = std::move(t1);

    printf("--- ② 传临时对象(值传参) ---\n");
    takeByValue(Tracked{});    // 构造一次直接绑定形参, 无拷贝无移动(C++17 强制消除)
    Tracked src;
    takeByValue(src);          // 具名对象值传参 → 拷贝

    printf("--- ③ vector 扩容: noexcept 移动 vs 拷贝 ---\n");
    std::vector<Tracked> v;
    v.reserve(1);
    v.emplace_back();          // count=1
    v.emplace_back();          // 触发扩容: 元素搬迁 —— 打印"移动构造"(有 noexcept)
    printf("(若移动构造没写 noexcept, 上一行会变成 拷贝构造)\n");

    printf("--- ④ 返回值 ---\n");
    Tracked r1 = makeValue();  // RVO: 打印 1 次默认构造, 零拷贝零移动
    Tracked r2 = makeBranch(false); // 分支返回: 一次移动
    return 0;
}

// 11 vector 扩容机制
// 运行: g++ -std=c++17 11_vector_growth.cpp -o t11 && ./t11
#include <cstdio>
#include <vector>

int main() {
    // ---- 1. capacity 增长轨迹 (libstdc++: 1→2→4→8...) ----
    std::vector<int> v;
    printf("初始: size=%zu cap=%zu\n", v.size(), v.capacity());
    for (int i = 1; i <= 17; ++i) {
        v.push_back(i);
        printf("push %2d 后: size=%2zu cap=%2zu %s\n",
               i, v.size(), v.capacity(),
               (v.size() == v.capacity()) ? "← 刚扩容" : "");
    }

    // ---- 2. reserve 一次到位 ----
    std::vector<int> r;
    r.reserve(17);
    printf("reserve(17) 后: size=%zu cap=%zu (size 不变, 只是容量就位)\n", r.size(), r.capacity());

    // ---- 3. 迭代器失效: 错误姿势 (UB, 仅注释示意) ----
    // for (auto it = v.begin(); it != v.end(); ++it)
    //     if (*it % 2 == 0) v.push_back(0);   // UB: push_back 可能扩容, it 立即作废
    // 正确姿势 1: 需要扩容的写法 —— 用下标, 每次重新取
    // 正确姿势 2: erase 的返回值是"下一个有效迭代器"
    std::vector<int> e{1, 2, 3, 4, 5, 6};
    for (auto it = e.begin(); it != e.end(); ) {
        if (*it % 2 == 0) it = e.erase(it);  // erase 返回下一个有效位置
        else ++it;
    }
    printf("删偶数后:");
    for (int x : e) printf(" %d", x);
    printf("\n");

    // ---- 4. resize vs reserve ----
    std::vector<int> s;
    s.resize(5);                 // 真的构造 5 个元素
    printf("resize(5) 后: size=%zu cap=%zu front=%d\n", s.size(), s.capacity(), s.front());
    return 0;
}

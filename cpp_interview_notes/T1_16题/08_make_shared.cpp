// 08 make_shared 比 new 好在哪
// 运行: g++ -std=c++17 08_make_shared.cpp -o t8 && ./t8
#include <cstdio>
#include <memory>

struct Big {
    long pad[4] = {1, 2, 3, 4};
};

int main() {
    // ---- 内存布局对比: 控制块与对象的相对位置 ----
    // shared_ptr<T>(new T): 两次分配 → 两块内存, 通常相距很远
    std::shared_ptr<Big> p1(new Big);
    // make_shared<T>(): 一次分配 → 对象紧挨着控制块
    auto p2 = std::make_shared<Big>();

    // &_get 的技巧不方便直接用, 这里用 use_count 的地址近似代表控制块位置:
    // (标准不保证可移植, 但 libstdc++ 上控制块就是紧邻对象, 地址差通常只有几十字节)
    printf("new 方式:      obj=%p\n", (void*)p1.get());
    printf("make_shared:   obj=%p  (与控制块同一次分配, 距离极近, 缓存友好)\n", (void*)p2.get());

    // ---- 异常安全窗口 (C++17 前真实存在) ----
    // int may_throw();
    // void f(std::shared_ptr<Big>, int);
    // f(std::shared_ptr<Big>(new Big), may_throw());
    //   ↑ C++17 前: "new Big" 先执行完, 随后 may_throw() 抛异常,
    //     shared_ptr 构造还没开始 → Big 泄漏。make_shared 无此缝隙。

    // ---- 代价: weak_ptr 拖住整块内存 ----
    std::weak_ptr<Big> w;
    {
        auto owned = std::make_shared<Big>();
        w = owned;
    }   // 对象逻辑上已死, 但 w 还观察着 → 整块内存(对象+控制块)都还不能 free
    printf("weak alive=%d (对象内存因此仍被占着)\n", (bool)w.lock());
    return 0;
}

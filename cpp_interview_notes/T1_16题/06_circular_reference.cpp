// 06 循环引用与 weak_ptr
// 运行: g++ -std=c++17 06_circular_reference.cpp -o t6 && ./t6
#include <cstdio>
#include <memory>

// ---- 泄漏版: 两条边都是强引用 ----
struct NodeLeak {
    std::shared_ptr<NodeLeak> other;
    ~NodeLeak() { printf("NodeLeak destroyed (你不会看到这行)\n"); }
};

// ---- 正确版: 反向边改 weak ----
struct Node {
    std::shared_ptr<Node> child;    // 父→子: 所有权方向, 用 shared
    std::weak_ptr<Node>   parent;   // 子→父: 反向引用, 用 weak
    ~Node() { printf("Node destroyed\n"); }
};

int main() {
    printf("--- 泄漏版: 互相 shared ---\n");
    {
        auto a = std::make_shared<NodeLeak>();
        auto b = std::make_shared<NodeLeak>();
        a->other = b;
        b->other = a;               // 强计数环: a,b 各被对方持着 1
    }                               // 作用域结束, 外部引用没了, 但两个计数都是 1 → 谁也不析构
    printf("作用域已结束, 但上面没有析构打印 → 泄漏\n");

    printf("--- 正确版: 反向边 weak ---\n");
    {
        auto parent = std::make_shared<Node>();
        auto child  = std::make_shared<Node>();
        parent->child = child;
        child->parent = parent;     // weak 不增持, 无环
        printf("child->parent.lock() 活着: %s\n", child->parent.lock() ? "yes" : "no");
    }                               // 正常析构, 两行 destroyed
    return 0;
}

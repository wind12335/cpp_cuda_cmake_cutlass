// 02 为什么基类析构函数要 virtual
// 运行: g++ -std=c++17 02_virtual_destructor.cpp -o t2 && ./t2
#include <cstdio>

// ---- 反例：析构不虚 ----
struct BaseBad {
    ~BaseBad() { printf("BaseBad::~BaseBad\n"); }
};
struct DerivedBad : BaseBad {
    int* buf = new int[1024];
    ~DerivedBad() {
        delete[] buf;               // 反例场景下永远不会执行 → 泄漏
        printf("DerivedBad::~DerivedBad\n");
    }
};

// ---- 正确：析构为虚 ----
struct BaseGood {
    virtual ~BaseGood() { printf("BaseGood::~BaseGood\n"); }
};
struct DerivedGood : BaseGood {
    int* buf = new int[1024];
    ~DerivedGood() override {
        delete[] buf;
        printf("DerivedGood::~DerivedGood\n");
    }
};

int main() {
    printf("--- 析构不虚: delete 只走基类析构 ---\n");
    BaseBad* bad = new DerivedBad;
    delete bad;                      // 只打印一行，Derived 的 buf 泄漏

    printf("--- 析构为虚: 先派生后基类 ---\n");
    BaseGood* good = new DerivedGood;
    delete good;                     // 两行都打印

    // 冷知识: shared_ptr 不需要虚析构也能正确调 Derived 析构（删除器记住了实际类型）
    return 0;
}

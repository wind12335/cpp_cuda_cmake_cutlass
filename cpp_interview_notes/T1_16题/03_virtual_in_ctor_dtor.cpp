// 03 构造/析构函数里调用虚函数会怎样
// 运行: g++ -std=c++17 03_virtual_in_ctor_dtor.cpp -o t3 && ./t3
#include <cstdio>

struct Base {
    Base()  { init(); }              // 构造时调虚函数：此时 vptr 指向 Base 的表
    virtual ~Base() { cleanup(); }   // 析构时调虚函数：Derived 部分已析构完，vptr 已改回 Base
    virtual void init()    { printf("Base::init\n"); }
    virtual void cleanup() { printf("Base::cleanup\n"); }
};

struct Derived : Base {
    void init()    override { printf("Derived::init\n"); }
    void cleanup() override { printf("Derived::cleanup\n"); }
    ~Derived() { cleanup(); }
};

int main() {
    printf("--- new Derived: 构造从 Base 开始逐层向上 ---\n");
    Derived* d = new Derived;   // 先 Base() 再 Derived()：init 打印的是 Base::init !

    printf("--- 正常多态调用（构造完成后）---\n");
    d->init();                  // 现在才是 Derived::init

    printf("--- delete: 析构从 Derived 开始逐层退回 ---\n");
    delete d;                   // Derived 部分先析构，然后 ~Base() 里 cleanup 打印 Base::cleanup
    return 0;
}
// 预期输出:
// Base::init            ← 构造中，虚分发只到 Base
// Derived::init         ← 构造完成后的正常多态
// Base::cleanup         ← 析构中（Derived 部分已拆完）
// BaseGood::~BaseGood   ← (对照 02 题)

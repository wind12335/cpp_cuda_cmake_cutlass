// 07 unique_ptr 为什么零开销
// 运行: g++ -std=c++17 07_unique_ptr.cpp -o t7 && ./t7
#include <cstdio>
#include <memory>
#include <vector>
#include <string>

struct Res {
    int v;
    explicit Res(int v) : v(v) { printf("Res(%d) 构造\n", v); }
    ~Res() { printf("Res(%d) 析构\n", v); }
};

int main() {
    // ---- 零开销验证: 尺寸 == 裸指针 ----
    printf("sizeof(int*)              = %zu\n", sizeof(int*));                          // 8
    printf("sizeof(unique_ptr<Res>)   = %zu\n", sizeof(std::unique_ptr<Res>));          // 8
    printf("sizeof(unique_ptr+无捕获lambda删除器) = %zu\n",
           sizeof(std::unique_ptr<Res, void(*)(Res*)>));                            // 8 (函数指针会被存进去, 演示见下)
    // 有状态删除器才会变大:
    auto stateful = [tag = 1](Res* p) { delete p; (void)tag; };
    printf("sizeof(unique_ptr+有状态lambda删除器) = %zu\n",
           sizeof(std::unique_ptr<Res, decltype(stateful)>));                       // 16

    // ---- 独占: 拷贝是编译错误 ----
    auto a = std::make_unique<Res>(1);
    // auto b = a;                    // ← 取消注释直接编译失败: deleted function
    auto b = std::move(a);            // 所有权转移: 只是指针值移动, 无任何计数操作
    printf("a 现在是 %s, b 持有 v=%d\n", a ? "非空" : "空", b->v);

    // ---- 容器用法: vector<unique_ptr> 是资源管理的标准姿势 ----
    std::vector<std::unique_ptr<Res>> v;
    v.push_back(std::make_unique<Res>(2));   // 移动进容器
    v.push_back(std::make_unique<Res>(3));
    for (auto& p : v) printf("容器中 v=%d\n", p->v);
    v.clear();                               // 统一析构

    // ---- 追问点: unique_ptr<Base> 管派生类, 基类析构必须 virtual ----
    struct Base { virtual ~Base() = default; };
    struct Derived : Base { ~Derived() override { printf("Derived 正确析构\n"); } };
    std::unique_ptr<Base> bp = std::make_unique<Derived>();
    return 0;
}

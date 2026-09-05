// 01 虚函数怎么实现多态
// 运行: g++ -std=c++17 -O0 01_virtual_polymorphism.cpp -o t1 && ./t1
#include <cstdio>

struct Animal {
    virtual void speak() { printf("Animal speaks\n"); }
    virtual ~Animal() = default;   // 系列第 2 题的主角
};

struct Dog : Animal {
    void speak() override { printf("Woof\n"); }
};

struct Cat : Animal {
    void speak() override { printf("Meow\n"); }
};

// 不含虚函数的普通类，用于对比对象大小
struct Plain { int x; };

int main() {
    // 同一行代码，运行期决定调谁 —— 这就是动态多态
    Animal* a = new Dog;
    Animal* b = new Cat;
    a->speak();   // Woof  ← 查 Dog 的 vtable
    b->speak();   // Meow ← 查 Cat 的 vtable
    delete a; delete b;

    // 对象头部的 vptr 是有代价的：x64 上多 8 字节
    printf("sizeof(Plain)  = %zu\n", sizeof(Plain));   // 4  （只有 int）
    printf("sizeof(Animal) = %zu\n", sizeof(Animal));  // 8  （只有 vptr，虚函数本身不占对象）
    printf("sizeof(Dog)    = %zu\n", sizeof(Dog));      // 8  （派生类复用基类的 vptr，不会再加）
    return 0;
}

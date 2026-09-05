// T3 31-38 速览题的可运行验证 (一张文件全包)
// 运行: g++ -std=c++17 31_misc_demos.cpp -o t31 && ./t31
#include <cstdio>
#include <string>
#include <utility>
#include <functional>

// ============ 31 菱形继承与虚继承 ============
struct A { int a = 1; explicit A(int x) : a(x) {} };
struct B : virtual A { explicit B(int x) : A(x) {} };   // virtual: A 在最终对象里只留一份
struct C : virtual A { explicit C(int x) : A(x) {} };
struct D : B, C {
    // 虚基类由"最派生类"直接初始化 (B/C 里的 A(x) 被忽略)
    explicit D(int x) : A(x), B(x + 1), C(x + 2) {}
};
void demo31() {
    D d(10);
    printf("31) sizeof(A)=%zu, sizeof(D)=%zu, d.a=%d (只有一份 A, 由 D 直接初始化)\n",
           sizeof(A), sizeof(D), d.a);   // D > A + B、C 的虚基类指针等
}

// ============ 32 模板放头文件 ============
template <class T>
T twice(T v) { return v * 2; }          // 定义必须在头文件 (此处单文件演示)
// 若定义放 .cpp: 本文件实例化 twice<int> 时看不到定义 → 链接错误
void demo32() { printf("32) twice(21)=%d, twice(1.5f)=%.1f\n", twice(21), twice(1.5f)); }

// ============ 33 this 指针本质 ============
struct Chain {
    int v = 0;
    Chain& add(int x) { v += x; return *this; }   // 返回 *this → 链式调用
    void show() const { printf("33) v=%d (this=%p)\n", v, (void*)this); }
};
void demo33() { Chain c; c.add(1).add(2).add(3).show(); }

// ============ 34 friend ============
class Secret {
    friend std::string peek(const Secret&);   // 声明朋友
    int code = 7331;
};
std::string peek(const Secret& s) { return "code=" + std::to_string(s.code); }  // 访问私有
void demo34() { Secret s; printf("34) friend 访问私有: %s\n", peek(s).c_str()); }

// ============ 35 decltype / auto ============
void demo35() {
    const int& r = 42;
    auto a = r;                    // 按值推导: 丢引用丢 const → int
    decltype(r) b = r;             // 声明类型: const int&
    int x = 1;
    decltype(x) c = x;             // int
    decltype((x)) d = x;           // 经典: 多层括号 → int&
    d = 99;
    printf("35) a=%d(副本) d 修改穿透: x=%d\n", a, x);
}

// ============ 36 完美转发 ============
struct Widget {
    Widget() = default;
    Widget(const Widget&)  { puts("36)   拷贝构造"); }
    Widget(Widget&&) noexcept { puts("36)   移动构造"); }
};
template <class T>
void relay(T&& arg) { Widget w = std::forward<T>(arg); }   // 还原值类别
void demo36() {
    puts("36) 传右值:");
    Widget src;
    relay(src);                 // 左值 → 拷贝
    relay(Widget{});            // 右值 → 移动 (forward 还原了右值性)
}

// ============ 37 函数指针 vs std::function ============
int add3(int x) { return x + 3; }
void demo37() {
    int (*fp)(int) = add3;                       // 函数指针: 只能装"签名匹配的裸函数"
    std::function<int(int)> f = add3;            // 类型擦除: 什么可调用物都能装
    int bias = 10;
    f = [bias](int x) { return x + bias; };      // 有捕获 lambda: 函数指针装不下
    printf("37) fp=%d, std::function(带捕获lambda)=%d\n", fp(1), f(1));
}

int main() {
    demo31(); demo32(); demo33(); demo34(); demo35(); demo36(); demo37();
    // 38) C++11 十特性: 见 T3_八题速览.md 第 38 节 (口径题, 无代码)
    return 0;
}

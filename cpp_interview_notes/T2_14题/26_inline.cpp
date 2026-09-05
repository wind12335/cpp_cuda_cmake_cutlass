// 26 inline: 链接语义 + 展开请求 + 与宏的区别
// 运行: g++ -std=c++17 26_inline.cpp -o t26 && ./t26
#include <cstdio>

// ---- 宏的副作用陷阱 ----
#define MAX_MACRO(a, b) ((a) > (b) ? (a) : (b))

// ---- inline 函数: 同样的逻辑, 类型安全, 参数只求值一次 ----
inline int max_inline(int a, int b) { return a > b ? a : b; }

// ---- inline 解决 ODR: 头文件里定义函数/变量的标准姿势 ----
inline int g_counter = 0;              // C++17 inline 变量: 多个 .cpp include 也只有一份
inline void bump() { ++g_counter; }    // inline 函数: 所有 TU 的定义合并为一份

struct Tracked {
    int v;
    explicit Tracked(int v) : v(v) { printf("  Tracked(%d) 构造\n", v); }
    ~Tracked() { printf("  Tracked(%d) 析构\n", v); }
};

int main() {
    // ---- 宏陷阱: 副作用参数被求值两次 ----
    int a = 3, b = 5;
    printf("宏   MAX(%d++, %d) = %d, 之后 a=%d (被自增两次!)\n",
           a, b, MAX_MACRO(a++, b), a);            // a++ 求值两次: 比较 + 取值各一次

    int c = 3, d = 5;
    printf("函数 MAX(%d++, %d) = %d, 之后 c=%d (正常自增一次)\n",
           c, d, max_inline(c++, d), c);

    // ---- inline 变量/函数: 就像在头文件里定义的全局共享状态 ----
    bump(); bump(); bump();
    printf("g_counter = %d (三个 TU 共享同一份的语义演示)\n", g_counter);

    // ---- 展开与否是编译器的自由 ----
    Tracked t{1};                 // 大构造函数: 即使标 inline 也会被拒绝展开
    (void)t;
    // 验证手段: g++ -S 看汇编, 或 godbolt 对比 -O0/-O2 下 max_inline 是否被内联成一条 cmp/cmov
    return 0;
}

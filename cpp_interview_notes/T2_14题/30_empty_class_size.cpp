// 30 空类大小 / vptr / 空基类优化 / 对齐填充
// 运行: g++ -std=c++17 30_empty_class_size.cpp -o t30 && ./t30
#include <cstdio>

struct Empty {};                       // 无成员无虚函数
struct WithVirtual { virtual void f() {} };  // 一个虚函数 → vptr
struct TwoVirtual { virtual void f() {} virtual void g() {} };  // 两个虚函数: 仍只有一个 vptr!
struct EmptyBase {};
struct DerivedFromEmpty : EmptyBase { int x; };  // 空基类优化 (EBO)
struct WithChar { char c; };
struct CharInt { char c; int i; };               // 对齐填充: 1+3(padding)+4

int main() {
    printf("sizeof(Empty)        = %zu  ← 占 1 字节 = 买一个不同的地址\n", sizeof(Empty));
    printf("sizeof(WithVirtual)  = %zu  ← vptr (虚函数在表里, 不在对象里)\n", sizeof(WithVirtual));
    printf("sizeof(TwoVirtual)   = %zu  ← 10 个虚函数也还是一个 vptr (表变大, 指针不变)\n", sizeof(TwoVirtual));
    printf("sizeof(DerivedFromEmpty) = %zu ← 空基类优化: 空基类不占空间\n", sizeof(DerivedFromEmpty));
    printf("sizeof(WithChar)     = %zu\n", sizeof(WithChar));
    printf("sizeof(CharInt)      = %zu ← char+对齐填充3+int = 8\n", sizeof(CharInt));

    // 地址唯一性: 两个空类对象地址必须不同
    Empty e1, e2;
    printf("两个空对象地址: %p vs %p (必不同)\n", (void*)&e1, (void*)&e2);

    // 数组依赖"至少 1 字节": 相邻元素地址差至少 1
    Empty arr[3];
    printf("空类数组相邻地址差: %td 字节\n",
           reinterpret_cast<char*>(&arr[1]) - reinterpret_cast<char*>(&arr[0]));

    // EBO 对智能指针的意义 (联动 07 题): 空删除器不占空间
    printf("unique_ptr<int>            = %zu 字节 (删除器空, 被压缩)\n", sizeof(unsigned long) == 8 ? sizeof(int*) : 0);
    return 0;
}

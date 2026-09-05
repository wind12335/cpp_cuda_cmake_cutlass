// 14 new/delete vs malloc/free
// 运行: g++ -std=c++17 14_new_vs_malloc.cpp -o t14 && ./t14
#include <cstdio>
#include <cstdlib>
#include <new>

struct Obj {
    explicit Obj(int v) { printf("  Obj(%d) 构造\n", v); }
    ~Obj() { printf("  Obj 析构\n"); }
};

// 池分配场景: 类级重载 operator new (GPU host 侧自定义分配器同款套路)
struct Pooled {
    static void* operator new(size_t sz) {
        void* p = std::malloc(sz);
        printf("  [Pooled::operator new] 分配 %zu 字节\n", sz);
        return p;
    }
    static void operator delete(void* p) {
        printf("  [Pooled::operator delete] 释放\n");
        std::free(p);
    }
};

int main() {
    printf("--- malloc vs new: 只有 new 调构造/析构 ---\n");
    Obj* raw = static_cast<Obj*>(std::malloc(sizeof(Obj)));   // 只有内存, 没有构造!
    printf("  (malloc 完, 注意上面没有构造打印, 这个'对象'处于未构造状态)\n");
    std::free(raw);

    Obj* p = new Obj(1);       // 分配 + 构造
    delete p;                  // 析构 + 释放

    printf("--- placement new: 在已有内存上构造 ---\n");
    alignas(Obj) unsigned char buf[sizeof(Obj)];
    Obj* pp = new (buf) Obj(2);     // 不分配内存, 只在 buf 上构造
    pp->~Obj();                     // 必须手动调析构 (placement new 没有 matching delete 表达式)

    printf("--- nothrow new: 失败返回 null 而非抛异常 ---\n");
    Obj* q = new (std::nothrow) Obj(3);
    if (q) delete q;

    printf("--- 类级 operator new 重载 ---\n");
    Pooled* pooled = new Pooled;    // 走上面的 Pooled::operator new
    delete pooled;

    // 配对纪律 (全部 UB, 不演示):
    // free(new 出来的) / delete(malloc 来的) / delete(new[] 出来的)
    return 0;
}

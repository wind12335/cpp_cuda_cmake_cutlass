// 15 堆 vs 栈
// 运行: g++ -std=c++17 15_heap_vs_stack.cpp -o t15 && ./t15 && ulimit -s
#include <cstdio>
#include <cstdlib>
#include <ctime>

struct Life {
    const char* tag;
    explicit Life(const char* t) : tag(t) { printf("  构造 %s @ %p\n", tag, (void*)this); }
    ~Life() { printf("  析构 %s @ %p\n", tag, (void*)this); }
};

int main() {
    // ---- 1. 地址区间: 栈在高位向下长, 堆在低位向上长 ----
    int stack_var = 1;
    int* heap_var = new int(2);
    printf("栈变量地址=%p  堆变量地址=%p\n", (void*)&stack_var, (void*)heap_var);
    delete heap_var;

    // ---- 2. 生命周期: 栈随作用域, 堆随手动/智能指针 ----
    printf("--- 进入作用域 ---\n");
    {
        Life on_stack("栈对象");                    // 作用域结束自动析构
        Life* on_heap = new Life("堆对象");
        (void)on_heap;
        // 故意不 delete, 观察输出顺序: 栈对象先析构, 堆对象没人析构 → 泄漏
    }
    printf("--- 作用域结束: 只析构了栈对象, 堆对象泄漏 ---\n");

    // ---- 3. 栈上大数组会爆栈 (UB, 不真跑) ----
    // int big[100'000'000];   // 400MB >> 8MB 线程栈 → SIGSEGV
    // 正确做法: 放堆上
    // auto* ok = new int[100'000'000]; delete[] ok;

    // ---- 4. 分配速度直观对比 (数量级感受) ----
    const int N = 1000000;
    // 栈: 移动指针, 不测了; 堆: malloc/free 一百万次
    clock_t c0 = clock();
    for (int i = 0; i < N; ++i) { void* p = std::malloc(64); std::free(p); }
    clock_t c1 = clock();
    printf("堆: %d 次 malloc+free 耗时 %.1f ms (栈分配本身≈0, 只有指针移动)\n",
           N, 1000.0 * (c1 - c0) / CLOCKS_PER_SEC);
    return 0;
}

// C03 实验: 指针/引用/栈堆/指针算术/void*/内存五区
// 运行: g++ -std=c++17 -O2 -Wall C03_pointers.cpp -o c03 && ./c03
#include <cstdio>

// §C03.9 内存五区演示用的"住户"
int g_global = 1;                    // 全局/静态区(.data)
static int s_file = 2;               // 全局区(static: 只有本文件可见)
void helloFunc() {}                  // 代码区(函数的机器码住这里)

// 演示: 三种"交换两个数"的写法
void swap_bad(int a, int b)        { int t = a; a = b; b = t; }   // 值传: 交换的是副本, 无效
void swap_ref(int& a, int& b)      { int t = a; a = b; b = t; }   // 引用: 有效
void swap_ptr(int* a, int* b)      { int t = *a; *a = *b; *b = t; } // 指针: 有效

int main() {
    // §C03.1 取地址与解引用
    int a = 42;
    int* p = &a;
    printf("a 的值=%d, a 的地址=%p, *p=%d\n", a, (void*)p, *p);
    *p = 100;                              // 通过指针改 a
    printf("通过指针改后 a=%d\n", a);

    // §C03.1.1 void* 通用指针: 只存地址, 不知道类型 → 解引用前必须转回
    void* vp = &a;                              // 任意对象指针 → void* (隐式, 安全)
    int* back = static_cast<int*>(vp);          // 转回 int* 才能解引用
    printf("void* 转回 int* 后 *back=%d\n", *back);

    // §C03.9 内存五区全景: 打印六类住户的地址, 对应 md 的地图
    static int s_local = 3;                     // 全局/静态区(static 局部: 有记忆)
    const char* literal = "rodata";             // 字面量 → 只读区
    int stack_var2 = 4;                         // 栈
    int* heap_var2 = new int(5);                // 堆
    printf("── 内存五区地址全景 ──\n");
    printf("代码区(函数)    %p\n", (void*)&helloFunc);
    printf("常量区(字面量)  %p\n", (void*)literal);
    printf("全局区(全局)    %p\n", (void*)&g_global);
    printf("全局区(static)  %p   &s_file=%p\n", (void*)&s_local, (void*)&s_file);
    printf("堆 heap         %p\n", (void*)heap_var2);
    printf("栈 stack        %p\n", (void*)&stack_var2);
    delete heap_var2;

    // §C03.4 交换三连
    int x = 1, y = 2;
    swap_bad(x, y);  printf("swap_bad : x=%d y=%d (无效! 交换了副本)\n", x, y);
    swap_ref(x, y);  printf("swap_ref : x=%d y=%d (成功)\n", x, y);
    swap_ptr(&x, &y);printf("swap_ptr : x=%d y=%d (成功)\n", x, y);

    // §C03.6/7 数组退化与指针算术
    int arr[5] = {10, 20, 30, 40, 50};
    int* q = arr;
    printf("arr[2]=%d, *(q+2)=%d, q 指向的地址=%p, q+1=%p (差 %zu 字节 = 一个 int)\n",
           arr[2], *(q + 2), (void*)q, (void*)(q + 1), sizeof(int));

    // §C03.5 栈与堆
    int stack_var = 1;                     // 栈
    int* heap_var = new int(7);            // 堆
    printf("栈地址=%p (高位), 堆地址=%p (低位), *heap=%d\n",
           (void*)&stack_var, (void*)heap_var, *heap_var);
    delete heap_var;                       // 忘了就是泄漏
    heap_var = nullptr;                    // 删完置空, 防悬垂

    // §C03.2 判空习惯
    int* maybe = nullptr;
    if (maybe) *maybe = 1; else printf("maybe 是空指针, 跳过使用\n");
    return 0;
}

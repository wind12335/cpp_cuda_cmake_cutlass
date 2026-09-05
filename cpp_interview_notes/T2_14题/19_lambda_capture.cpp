// 19 lambda 三种捕获的坑
// 运行: g++ -std=c++17 19_lambda_capture.cpp -o t19 && ./t19
#include <cstdio>
#include <memory>
#include <functional>
#include <vector>

std::function<void()> g_task;   // 全局"任务槽", 模拟异步执行环境

struct Worker {
    int id = 7;
    void bindBad()  { g_task = [this] { printf("  用 this 捕获, id=%d\n", id); }; }   // 捕获裸指针
    void bindGood() { g_task = [*this] { printf("  用 [*this] 拷贝对象, id=%d\n", id); }; } // C++17
};

int main() {
    // ---- 1. 按值捕获: 快照 ----
    int x = 1;
    auto byVal = [x]() mutable { x += 10; printf("  值捕获闭包内 x=%d\n", x); };
    byVal();
    printf("  外部 x=%d (拷贝, 互不影响)\n", x);

    // ---- 2. 按引用捕获: 立即调用安全 ----
    int y = 1;
    auto byRef = [&y] { y += 10; };
    byRef();
    printf("  引用捕获后外部 y=%d (就是同一个变量)\n", y);

    // ---- 3. 悬垂引用: 逃逸作用域才执行 → UB ----
    std::function<void()> bad;
    {
        int local = 42;
        bad = [&local] { printf("  local=%d\n", local); };  // 捕获的是对 local 的引用
    }                                                        // local 在这里销毁!
    // bad();   ← 打开就是 UB (栈内存已回收, 读到垃圾值或崩溃)

    // ---- 4. this 悬垂: 对象先死, 任务后跑 ----
    {
        Worker w;
        w.bindBad();            // g_task 持有 &w
    }                           // w 销毁
    printf("Worker 已销毁, 现在执行旧任务 (用 this 捕获 = 裸指针悬垂):\n");
    // g_task();   ← UB. 修复: bindGood() 用 [*this] 拷贝整个对象

    {
        Worker w2;
        w2.bindGood();
        g_task();               // 安全: 闭包里是 w2 的一份拷贝
    }

    // ---- 5. C++14 初始化捕获: 把 move 带进捕获 ----
    auto p = std::make_unique<int>(99);
    auto owner = [q = std::move(p)] { printf("  闭包独占 *q=%d\n", *q); };  // 所有权移进闭包
    owner();
    printf("  外部 p=%s\n", p ? "非空" : "空(已被移走)");
    return 0;
}

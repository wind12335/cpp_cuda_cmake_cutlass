// 21 static 的全部用法
// 运行: g++ -std=c++17 -pthread 21_static.cpp -o t21 && ./t21
#include <cstdio>
#include <thread>
#include <vector>

// ---- ① 局部 static: Meyers 单例 (C++11 起线程安全) ----
struct Config {
    int value = 42;
    static Config& instance() {
        static Config inst;       // 首次调用才构造; 多线程同时首次调用也只构造一次 (magic static)
        return inst;
    }
};

// ---- ②③ 类的 static 成员 ----
struct Counter {
    static inline int total = 0;  // C++17 inline 变量: 免去类外定义 (传统写法要 int Counter::total = 0;)
    int local = 0;
    void hit() { ++total; ++local; }              // 静态成员被所有实例共享
    static int total_count() { return total; }    // 静态成员函数: 没有 this, 不能碰 local
};

// ---- ④ 全局 static = 内部链接 ----
static int file_local = 1;        // 只在本 .cpp 可见, 其他编译单元 extern 也看不到
// (验证: 另写 a.cpp 声明 extern int file_local; 链接报 undefined reference)

int main() {
    printf("--- ① Meyers 单例 (并发首次调用) ---\n");
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < 8; ++i)
            ts.emplace_back([] { Config::instance().value += 1; });
        for (auto& t : ts) t.join();
        printf("  8 线程并发初始化后 value=%d (构造只发生一次, 42+8)\n", Config::instance().value);
    }

    printf("--- ② static 成员变量: 所有对象共享 ---\n");
    Counter a, b;
    a.hit(); a.hit(); b.hit();
    printf("  a.local=%d b.local=%d total=%d\n", a.local, b.local, Counter::total);

    printf("--- ③ 静态成员函数没有 this ---\n");
    printf("  Counter::total_count()=%d (不需要实例也能调)\n", Counter::total_count());
    return 0;
}

// 17 深拷贝 vs 浅拷贝 —— md 名词 ↔ 代码实体对照:
//   "浅拷贝(默认合成拷贝)"   → struct Shallow: 没写拷贝构造, 编译器自动生成的那个
//   "两把钥匙一间房"          → a.data == b.data (同一块内存, 打印地址可证)
//   "拆两次就塌(double free)"→ ~Shallow 里被注释掉的 delete[] (打开就崩)
//   "深拷贝"                 → struct Deep: 手写的拷贝构造 (new 新内存 + strcpy 内容)
//   "Rule of Three"          → Deep 写全了 析构/拷贝构造/拷贝赋值 三件套
//   "Rule of Zero"           → struct Zero: 用 string/unique_ptr 管资源, 五个特殊成员全不写
// 运行: g++ -std=c++17 17_deep_shallow_copy.cpp -o t17 && ./t17
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// ---- 反例: 默认浅拷贝 (真跑会 double free, 这里用静态计数演示后果) ----
struct Shallow {
    char* data;
    static int alive;
    explicit Shallow(const char* s) : data(new char[strlen(s) + 1]) {
        strcpy(data, s); ++alive;
        printf("  Shallow 构造 \"%s\"\n", s);
    }
    // 编译器合成的拷贝构造 ≈ data = other.data; ++alive;  ← 两对象同指一块内存
    ~Shallow() {
        --alive;
        printf("  Shallow 析构 \"%s\", 剩余持有=%d %s\n", data, alive,
               alive > 0 ? "← 另一个对象还在用它! double free 预定" : "");
        // delete[] data;   ← 打开就是真 double free 崩溃, 保持注释
    }
};
int Shallow::alive = 0;

// ---- 正确版 1: 手写深拷贝 (Rule of Three) ----
struct Deep {
    char* data;
    explicit Deep(const char* s) : data(new char[strlen(s) + 1]) { strcpy(data, s); }
    Deep(const Deep& o) : data(new char[strlen(o.data) + 1]) { strcpy(data, o.data); }   // 复制内容!
    Deep& operator=(const Deep& o) {
        if (this != &o) {
            char* nd = new char[strlen(o.data) + 1];
            strcpy(nd, o.data);
            delete[] data;
            data = nd;
        }
        return *this;
    }
    ~Deep() { delete[] data; }
};

// ---- 正确版 2: Rule of Zero —— 用成员管资源, 五个特殊成员全不写 ----
struct Zero {
    std::string data;                       // string 自己会深拷贝
    std::unique_ptr<int> ptr = std::make_unique<int>(7);  // unique_ptr 禁拷贝 → Zero 天然不可拷贝
    explicit Zero(std::string s) : data(std::move(s)) {}
};

int main() {
    printf("--- 浅拷贝的问题 ---\n");
    {
        Shallow a("shared");
        Shallow b = a;          // 合成的浅拷贝: b.data == a.data
        printf("  a.data=%p b.data=%p (同一块!)\n", (void*)a.data, (void*)b.data);
    }                           // 析构两次同一块 data → 第二次是 double free (此处仅打印)
    printf("作用域结束, 泄漏演示完毕\n\n");

    printf("--- 深拷贝 ---\n");
    {
        Deep a("owner");
        Deep b = a;             // 各有一份内容
        printf("  a.data=%p b.data=%p (不同内存, 内容相同)\n", (void*)a.data, (void*)b.data);
    }
    printf("正常析构两次, 无泄漏\n\n");

    printf("--- Rule of Zero ---\n");
    {
        Zero a("modern");
        // Zero b = a;           // unique_ptr 成员 → 拷贝被禁, 想共享请显式 make 新的
        Zero b("another");
    }
    return 0;
}

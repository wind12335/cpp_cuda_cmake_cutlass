// 27 explicit: 禁隐式转换
// 运行: g++ -std=c++17 27_explicit.cpp -o t27 && ./t27
#include <cstdio>
#include <string>

struct DeviceId {
    int id;
    explicit DeviceId(int i) : id(i) {}                    // explicit: 关闭 int→DeviceId 通道
};

struct StreamName {
    std::string name;
    StreamName(const char* s) : name(s) {}                 // 不 explicit: 刻意允许 "xx" → StreamName
};

void launch(DeviceId dev) { printf("  launch on device %d\n", dev.id); }
void tag(StreamName s)    { printf("  tag = %s\n", s.name.c_str()); }

struct Pixels {
    int n;
    explicit Pixels(int n) : n(n) {}
};

int main() {
    launch(DeviceId(0));        // ✓ 显式构造
    // launch(0);               // ✗ 编译错: 不允许 int → DeviceId 隐式转换 (取消注释试试)
    // DeviceId d = 1;          // ✗ 同样被拦

    tag("default_stream");      // ✓ 自然语义, 刻意不 explicit
    StreamName s2 = "aux";      // ✓ 拷贝初始化也走隐式转换 (非 explicit 才有)

    // explicit 对多参构造/列表初始化的效果 (C++11+):
    // Pixels p = {1024};       // ✗ explicit 拦截列表形式的隐式转换
    Pixels p{1024};             // ✓ 直接初始化不拦
    printf("Pixels n=%d (直接初始化合法)\n", p.n);

    // explicit operator bool: 只在"条件语境"转换
    struct Guard {
        explicit operator bool() const { return true; }
    };
    Guard g;
    if (g) printf("explicit operator bool: if 中可用\n");
    // int x = g;               // ✗ 编译错: 不许荒唐地转 int
    bool ok = static_cast<bool>(g);   // ✓ 显式可以
    (void)ok;
    return 0;
}

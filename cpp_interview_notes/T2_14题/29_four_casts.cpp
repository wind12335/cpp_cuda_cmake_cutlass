// 29 四种 cast
// 运行: g++ -std=c++17 29_four_casts.cpp -o t29 && ./t29
#include <cstdio>
#include <string>
#include <typeinfo>

struct Base { virtual ~Base() = default; };
struct Derived : Base { int extra = 42; };
struct Unrelated : Base { };

int main() {
    // ---- ① static_cast: 数值转换 / void* 还原 ----
    double d = static_cast<double>(7) / 2;              // 整数除法→浮点
    printf("static_cast 数值: 7/2 = %.1f\n", d);
    int* pi = new int(5);
    void* pv = pi;
    int* back = static_cast<int*>(pv);                  // void* → int* (编译期可判定)
    printf("static_cast 指针还原: *back=%d\n", *back);
    delete pi;

    // ---- ② dynamic_cast: 多态下行, 运行期检查 ----
    Base* p = new Derived;
    if (Derived* dp = dynamic_cast<Derived*>(p))        // 实际是 Derived → 成功
        printf("dynamic_cast 成功: extra=%d\n", dp->extra);
    // Unrelated* up = dynamic_cast<Unrelated*>(p);     // 实际不是 Unrelated → 返回 nullptr (不崩)
    Base& ref = *p;
    try { (void)dynamic_cast<Derived&>(ref); printf("引用版转换成功\n"); }
    catch (const std::bad_cast&) { printf("引用版失败抛 bad_cast\n"); }
    delete p;

    // ---- ③ const_cast: 去 const (慎用) ----
    const std::string msg = "legacy api";
    // 合法场景: 老接口签名忘了 const, 但承诺不改
    size_t legacy_read(const char* s);                  // 声明: 老 C 风格接口
    (void)legacy_read;
    printf("const_cast 示例: msg 长度 %zu (传给老接口时去 const)\n",
           legacy_read(msg.c_str()));                   // c_str 本身返回 const char*, 老接口要 char* 的场景才用

    // ---- ④ reinterpret_cast: 位模式重解释 ----
    long handle = 0x1234;
    int* reint = reinterpret_cast<int*>(handle);        // 整数→指针, 平台相关
    printf("reinterpret_cast 演示 (只打印不解引用, 解引用多半段错误): %p\n", (void*)reint);
    // 危险示例: 把 float 指针当 int 指针读 → 违反严格别名规则, UB
    // float f = 1.0f; int* ip = reinterpret_cast<int*>(&f); printf("%d", *ip);  // UB!
    // 合法替代: memcpy / std::bit_cast (C++20)
    return 0;
}
size_t legacy_read(const char* s) { return __builtin_strlen(s); }

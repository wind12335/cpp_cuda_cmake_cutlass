// C01 实验: 变量/类型/位运算 —— 每段对应 md 的一个小节
// 运行: g++ -std=c++17 -O2 C01_basics.cpp -o c01 && ./c01
#include <cstdio>
#include <type_traits>
#include <string>

int main() {
    // §C01.1 类型与大小
    printf("int=%zuB float=%zuB double=%zuB long long=%zuB size_t=%zuB\n",
           sizeof(int), sizeof(float), sizeof(double), sizeof(long long), sizeof(size_t));

    // §C01.2 初始化: 列表初始化禁止窄化
    int a = 3.14;            // 静默截断成 3 (危险)
    // int b{3.14};          // ← 打开注释就是编译错误: 列表初始化帮你抓 bug
    printf("int a = 3.14 的结果是 %d (丢了小数)\n", a);

    // §C01.4 类型转换
    double d = 7.9;
    int i = static_cast<int>(d);
    printf("static_cast<int>(%.1f) = %d (向零截断, 不是四舍五入)\n", d, i);

    // §C01.5 位运算 (CUDA 高频) + 进制前缀
    int x = 0b1011;                        // 二进制 1011 = 11
    printf("x=%d, 最低位=%d (x&1), x<<1=%d (乘2), x&0xFF=%d\n",
           x, x & 1, x << 1, x & 0xFF);
    printf("进制前缀: 0b1011=%d, 0xFF=%d, 010=%d ← 开头一个0是八进制: 010 是 8 不是 10!\n",
           0b1011, 0xFF, 010);
    // §C01.5 与/或/异或 逐位验证例题 (你出的题!)
    unsigned p = 0b1011, q = 0b1100;
    auto bits8 = [](unsigned val) {                 // 把 8 位二进制打印成 "01011000" 的样子
        std::string s;
        for (int i = 7; i >= 0; --i) s += ((val >> i) & 1) ? '1' : '0';
        return s;                                   // std::string 按值返回, 安全(前置 C07 移动)
    };
    printf("0b1011 & 0b1100 = %s (=8)\n",  bits8(p & q).c_str());
    printf("0b1011 | 0b1100 = %s (=15)\n", bits8(p | q).c_str());
    printf("0b1011 ^ 0b1100 = %s (=7)\n",  bits8(p ^ q).c_str());
    int sz = 100;
    printf("把 %d 向上对齐到 256 的倍数 = %d  ← (sz+255)&~255 套路\n", sz, (sz + 255) & ~255);

    // §C01.7 整数除法陷阱
    printf("7/2 = %d  (整数除法截断!), 7/2.0 = %.1f  (有浮点参与才是真除法)\n", 7 / 2, 7 / 2.0);

    // §C01.1 无符号陷阱 (新补小节)
    unsigned u = 0;
    printf("unsigned 0 - 1 = %u  (不是 -1! 从 0 回绕到最大值)\n", u - 1);
    int sx = -1;
    unsigned v = 2;
    // ↓ 下面这行编译时 GCC 用 -Wall 会警告 "signedness 不同" —— 这个警告就是在救你!
    printf("把 sx=-1 转成 unsigned = %u, 于是 sx < v 为 %s (经典坑: 隐式转换!)\n",
           (unsigned)sx, (sx < v) ? "真" : "假");

    // §C01.1 补充: size_t vs uint64_t 辨析 (前置: C11 std::is_same)
    printf("size_t 真身是 unsigned long(Linux x64)? %s\n",
           std::is_same<size_t, unsigned long>::value ? "是" : "否");
    printf("size_t 和 uint64_t 同宽度可互换, 但语义不同: 前者=大小(随平台), 后者=精确64位\n");

    // §C01.6 enum class: 新类型 + 合法取值菜单 (定义时【不创建任何对象/变量】!)
    enum class Mode { Train, Infer };
    Mode m = Mode::Train;                 // 变量 m 在这里才出生, 取菜单项 Train
    printf("Mode::Train 的底层值 = %d\n", (int)m);   // 0 (菜单项默认从 0 递增)
    m = Mode::Infer;
    printf("切换后底层值 = %d\n", (int)m);           // 1
    // m = 5;                             // ✗ 打开注释: 编译错! 5 不在菜单上
    return 0;
}

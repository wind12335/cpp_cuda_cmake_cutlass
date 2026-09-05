// 01_exp_虚调用成本.cpp — 虚函数的性能代价: virtual vs CRTP vs 直接调用
// ═══════════════════════════════════════════════════════════════════
// 这个实验回答: "虚函数慢在哪? 慢多少?"
// 结论预览(本机 -O2 实测):
//   可内联直调   ~0.18 ns/次   ← 基准线
//   CRTP        ~0.18 ns/次   ← 编译期分发, 和直调一样快
//   虚调用(单态) ~0.9  ns/次   ← 调用点永远调同一个实现, 慢 5 倍
//   虚调用(多态) ~6.0  ns/次   ← 同一行轮流调不同实现, 慢 33 倍
//
// 编译: g++ -std=c++17 -O2 01_exp_虚调用成本.cpp -o exp && ./exp
// ═══════════════════════════════════════════════════════════════════

#include <cstdio>
#include <chrono>     // std::chrono: 标准库的高精度计时工具
#include <utility>    // std::pair

// ════════════════════════════════════════════════════════════════
// 写法 A: 经典虚函数 (运行期分发 —— 我们要测的主角)
// ════════════════════════════════════════════════════════════════
struct IVirtual {
    long x = 0;   // 成员变量: 让 tick 有真实工作可做, 否则编译器会把空函数优化没

    // tick 做一件极小的事: x 加上 i 的低 8 位
    // "i & 0xFF" 是位运算: 0xFF = 二进制 11111111, & 运算只保留 i 的低 8 位
    // 目的: 结果永远是小数字, 防止 long 溢出, 也让每次调用有真实的读写发生
    virtual void tick(int i) { x += i & 0xFF; }

    // 虚析构: 有虚函数的基类标配 (第 02 题的主角), 这里测性能可以不加, 但习惯要养成
    virtual ~IVirtual() = default;
};

// Dog 的实现: 和基类一模一样的逻辑
struct Dog : IVirtual {
    void tick(int i) override { x += i & 0xFF; }   // override: 声明"我在重写基类的虚函数"
};

// Cat 的实现: 故意和 Dog 不同 (x += i*3), 制造"不同子类做不同事"
// 它存在的意义: 让调用点轮流调用两个不同实现 → 测多态调用的额外代价
struct Cat : IVirtual {
    void tick(int i) override { x += (i * 3) & 0xFF; }
};

// ════════════════════════════════════════════════════════════════
// 写法 B: CRTP (Curiously Recurring Template Pattern, 奇特递归模板模式)
// 把"调用哪个函数"的决定从运行期挪到编译期 —— 没有虚表, 没有间接跳转
// ════════════════════════════════════════════════════════════════
// 基类是个模板, 模板参数 D 是"将来哪个类继承我"
template <class D>
struct CrtpBase {
    long x = 0;

    // 关键一行: 把自己强转成派生类 D 的指针, 直接调派生类的实现
    // 因为 D 在编译期就确定了, 这行会被编译器内联成"直接执行 tick_impl 的代码"
    // 没有 vptr、没有虚表查找、没有间接跳转 —— 这就是它和 virtual 的本质区别
    void tick(int i) { static_cast<D*>(this)->tick_impl(i); }
};

// 派生类把"自己"作为模板参数传给基类 —— 这就是"奇特递归"名字的由来
struct CrtpDog : CrtpBase<CrtpDog> {
    void tick_impl(int i) { x += i & 0xFF; }   // 逻辑和 Dog::tick 一模一样
};

// ════════════════════════════════════════════════════════════════
// 写法 C: 普通直接调用 (基准线, 没有任何分发机制)
// ════════════════════════════════════════════════════════════════
struct Direct {
    long x = 0;
    void tick(int i) { x += i & 0xFF; }   // 非虚函数: 编译期就知道调谁, 必然可内联
};

// ════════════════════════════════════════════════════════════════
// 计时辅助: 一段"测量某次调用花了多少纳秒/每次"的朴素代码
// ════════════════════════════════════════════════════════════════
// std::chrono::steady_clock: 单调时钟 (只会前进, 不受系统改时间影响), 测耗时用它
//   注意别用 system_clock —— 它可能被 NTP 校时, 往回跳
using clk = std::chrono::steady_clock;

// 两次时间点之间的纳秒数
// std::chrono::duration<double, std::nano>: 把时间差表示成"double 类型的纳秒"
static double elapsed_ns(clk::time_point t0, clk::time_point t1) {
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// ════════════════════════════════════════════════════════════════
// 三个被测函数 —— 每个都是"循环 N 次 tick", 结构完全相同, 只有分发方式不同
// ════════════════════════════════════════════════════════════════

// 这里有两个防止编译器"帮倒忙"的手段, 是基准测试的防御性写作:

// 手段 1: __attribute__((noinline)) —— GCC/Clang 扩展(MSVC 用 __declspec(noinline)), 禁止内联此函数
//   为什么想禁? 如果 run_virtual 被内联进 main, 编译器有机会顺着分析出"p 其实指向 Dog"
//   → 偷偷去虚化(devirtualization) → 虚调用变直调 → 我们就测不到虚调用了。
//
// 手段 2: 全局指针 g_v —— 隐藏真实类型
//   从全局指针取对象, 编译器在函数内无法证明 g_v 指向谁 → 无法安全去虚化。
//
// ⚠️ 实测备注(2026-09, GCC11 -O2 本机): 用 sed 删掉 noinline 再跑, 四个数字几乎不变;
//   反汇编(nm + objdump -d)显示: 外层函数确实被内联了(符号消失), 但虚调用仍以
//   call *(%rax) 间接调用形式保留 —— 即该编译器没有做去虚化。所以 noinline 在本机
//   不是命门而是保险: 换 MSVC/ICC 或开 -O3 -flto 时去虚化可能真的发生, 加上它才能
//   保证"被测对象原样到达被测位置"。教训: 别信"我以为编译器会怎样", 用反汇编验证。

static IVirtual* g_v = nullptr;    // 全局指针, 指向被测的虚函数对象

__attribute__((noinline))
static long run_virtual_mono(long n) {
    IVirtual* p = g_v;                       // 从全局指针取 → 编译期不知道指向 Dog 还是 Cat
    for (long i = 0; i < n; ++i)
        p->tick((int)i);                     // ← 被测对象: 虚调用 (查 vptr → 虚表 → 间接跳转)
    return p->x;                             // 返回结果: 给 main 打印, 防止整个循环被"没用到"删掉
}

static IVirtual* g_cat = nullptr;            // 第二个虚函数对象 (Cat)

__attribute__((noinline))
static long run_virtual_poly(long n) {
    IVirtual* ps[2] = {g_v, g_cat};          // 两个不同子类的对象
    for (long i = 0; i < n; ++i)
        ps[i & 1]->tick((int)i);             // ← "i & 1" = 取 i 的最低位: i 是偶数取 0(用 Dog), 奇数取 1(用 Cat)
                                             //    同一行代码轮流调两个实现 → 分支预测器猜不中 → 额外罚时
    return g_v->x + g_cat->x;
}

static CrtpDog g_c;

__attribute__((noinline))
static long run_crtp(long n) {
    CrtpDog* p = &g_c;                       // 具体类型可见 → CRTP 的 tick 在编译期已解析,
    for (long i = 0; i < n; ++i)             //    编译器直接把 tick_impl 内联进循环 → 只剩纯加法
        p->tick((int)i);
    return p->x;
}

static Direct g_d;

__attribute__((noinline))
static long run_direct(long n) {
    Direct* p = &g_d;                        // 具体类型 + 非虚 → 同样被内联, 和 CRTP 一样快
    for (long i = 0; i < n; ++i)
        p->tick((int)i);
    return p->x;
}

// ════════════════════════════════════════════════════════════════
// main: 依次测量四种情况, 打印"每次调用多少纳秒"
// ════════════════════════════════════════════════════════════════
int main() {
    const long N = 200'000'000;   // C++14 数字分隔符: 单引号让大数字可读, 等价 200000000

    Dog dog;      g_v = &dog;    // 单态实验: g_v 指向 Dog
    Cat cat;      g_cat = &cat;  // 多态实验的两个演员就位

    // ---- 1. 虚调用(单态): 调用点永远调同一个实现 ----
    clk::time_point t0 = clk::now();
    long rV = run_virtual_mono(N);
    clk::time_point t1 = clk::now();
    double nsV = elapsed_ns(t0, t1) / N;

    // ---- 2. CRTP: 编译期分发 ----
    t0 = clk::now();
    long rC = run_crtp(N);
    t1 = clk::now();
    double nsC = elapsed_ns(t0, t1) / N;

    // ---- 3. 直接调用(可内联): 基准线 ----
    t0 = clk::now();
    long rD = run_direct(N);
    t1 = clk::now();
    double nsD = elapsed_ns(t0, t1) / N;

    // ---- 4. 虚调用(多态): 同一行轮流调 Dog/Cat ----
    t0 = clk::now();
    long rP = run_virtual_poly(N);
    t1 = clk::now();
    double nsP = elapsed_ns(t0, t1) / N;

    // 打印结果。打印校验值 rV/rC/rD/rP 很重要:
    // 如果结果从不被使用, 编译器有权把"纯计算"循环整个删除 —— 测了个寂寞
    printf("virtual 单态调用 : %6.2f ns/次 (校验 %ld)  调用点永远同一实现, 预测器猜得中\n", nsV, rV);
    printf("CRTP   编译期   : %6.2f ns/次 (校验 %ld)  编译期解析, 无 vptr 无虚表, 可内联\n", nsC, rC);
    printf("direct 可内联   : %6.2f ns/次 (校验 %ld)  普通函数内联后的地板价\n", nsD, rD);
    printf("virtual 多态调用 : %6.2f ns/次 (校验 %ld)  同一行轮流调不同实现, 分支预测翻车\n", nsP, rP);

    printf("\n解读: 三个成本从小到大 = 查表本身(小) < 分支预测失败(中) < 无法内联挡住后续优化(大);\n");
    printf("      判断标准只有一条: 每次虚调用背后的工作量是否远大于 1-2ns。\n");
    printf("      每 kernel/每层分发一次 → 随便用; 热循环里逐元素分发 → 用模板/CRTP 挪到编译期。\n");
    return 0;
}

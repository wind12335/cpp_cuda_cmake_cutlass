// C05 实验: 类/继承/多态/运算符重载
// 运行: g++ -std=c++17 -O2 C05_classes.cpp -o c05 && ./c05
#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include <utility>    // std::move

// ---- §C05.1/5.5 Vec3: 封装 + 运算符重载 ----
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    float norm() const { return std::sqrt(dot(*this)); }
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator*(float k) const { return {x*k, y*k, z*k}; }
};

// ---- §C05.3/5.4 继承 + 多态: 统一的"kernel"接口 ----
struct Kernel {
    virtual void run() { printf("  base run\n"); }
    virtual ~Kernel() = default;          // 基类析构必须 virtual(面试 02 题)
};
struct GemmKernel : Kernel {
    void run() override { printf("  gemm run\n"); }
};
struct AttnKernel : Kernel {
    void run() override { printf("  attn run\n"); }
};

// 多态的价值: 一个函数吃"任何 Kernel", 不用为每种 kernel 写一遍
void bench(Kernel& k) { k.run(); }

// ---- §C05.2.1 noexcept 实测: 同一个类, 移动构造贴不贴 noexcept, 扩容行为天差地别 ----
// 两个类代码完全一样, 唯一区别: 移动构造有没有 noexcept。
// 用静态计数器数"拷贝了几次/移动了几次", 眼见为实。
struct ThrowMove {                     // 移动"没签合同"(没 noexcept)
    static int copies, moves;
    int* payload;
    ThrowMove() : payload(new int[100]) {}
    ~ThrowMove() { delete[] payload; }
    ThrowMove(const ThrowMove& o) : payload(new int[100]) { ++copies; }   // 拷贝
    ThrowMove(ThrowMove&& o) : payload(o.payload) { o.payload = nullptr; ++moves; }  // 移动(会抛的嫌疑)
};
struct NoThrowMove {                  // 移动"签了合同"(noexcept)
    static int copies, moves;
    int* payload;
    NoThrowMove() : payload(new int[100]) {}
    ~NoThrowMove() { delete[] payload; }
    NoThrowMove(const NoThrowMove& o) : payload(new int[100]) { ++copies; }
    NoThrowMove(NoThrowMove&& o) noexcept : payload(o.payload) { o.payload = nullptr; ++moves; }
};
int ThrowMove::copies = 0, ThrowMove::moves = 0;
int NoThrowMove::copies = 0, NoThrowMove::moves = 0;

int main() {
    // Vec3 运算符重载
    Vec3 a(1, 2, 3), b(4, 5, 6);
    Vec3 c = a + b * 2.0f;
    printf("a+b*2 = (%.0f, %.0f, %.0f), |a| = %.3f, a·b = %.0f\n",
           c.x, c.y, c.z, a.norm(), a.dot(b));

    // 多态: 基类指针数组装不同派生类 —— 运行期自动分发
    GemmKernel gemm; AttnKernel attn;
    Kernel* kernels[] = {&gemm, &attn};
    printf("多态分发:\n");
    for (Kernel* k : kernels) bench(*k);

    // Rule of Zero: 智能指针(前置 C06) + 多态 = 工程标准姿势
    std::unique_ptr<Kernel> owned = std::make_unique<AttnKernel>();
    owned->run();     // 离开作用域自动 delete, 无泄漏

    // ---- noexcept 实测: 往 vector 里塞 1000 个, 数扩容搬家用的是拷贝还是移动 ----
    {
        std::vector<ThrowMove> v;
        for (int i = 0; i < 1000; ++i) v.push_back(ThrowMove{});   // 临时对象→塞进去
        printf("移动构造【没】noexcept: 扩容搬家中 拷贝=%d 次, 移动=%d 次 (vector 不敢用移动!)\n",
               ThrowMove::copies, ThrowMove::moves);
    }
    {
        std::vector<NoThrowMove> v;
        for (int i = 0; i < 1000; ++i) v.push_back(NoThrowMove{});
        printf("移动构造【有】noexcept: 扩容搬家中 拷贝=%d 次, 移动=%d 次 (全走移动, 快)\n",
               NoThrowMove::copies, NoThrowMove::moves);
    }
    return 0;
}

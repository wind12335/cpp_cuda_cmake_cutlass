// C05 实验: 类/继承/多态/运算符重载
// 运行: g++ -std=c++17 -O2 C05_classes.cpp -o c05 && ./c05
#include <cstdio>
#include <cmath>
#include <memory>

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
    return 0;
}

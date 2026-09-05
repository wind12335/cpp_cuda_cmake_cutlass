// 28 override / final
// 运行: g++ -std=c++17 28_override_final.cpp -o t28 && ./t28
#include <cstdio>

struct Shape {
    virtual void draw() const { printf("  Shape::draw\n"); }
    virtual double area() const { return 0; }
    virtual ~Shape() = default;
};

struct Circle : Shape {
    // ---- 反例: 签名不一致, 没有 override 时是"静默的新函数" ----
    // void draw() { printf("...\n"); }        // ← 漏了 const! 这是 Circle 的新函数, 不重写 Shape::draw
    // double area(double extra) { ... }       // ← 多了参数, 同样静默不重写
    // 加上 override 后, 上面两行直接编译报错 —— 这就是它的价值

    void draw() const override { printf("  Circle::draw\n"); }
    double area() const override { return 3.14 * r * r; }
    explicit Circle(double r) : r(r) {}
private:
    double r;
};

// ---- final 修饰虚函数: 封顶, 不得再重写 ----
struct UnitCircle final : Circle {          // final 修饰类: 不得被继承
    explicit UnitCircle() : Circle(1.0) {}
    void draw() const final { printf("  UnitCircle::draw (final)\n"); }
};

// struct BigCircle : UnitCircle {};         // ✗ 取消注释: 编译错, UnitCircle 是 final
// struct Weird : Circle { void draw() const override; }  // Circle 非 final, 这个可以

double printedArea(const Circle& c) { return c.area(); }  // 去虚化机会: UnitCircle 场景可被内联

int main() {
    Circle c(2.0);
    UnitCircle u;
    Shape& s1 = c; s1.draw();               // Circle::draw
    Shape& s2 = u; s2.draw();               // UnitCircle::draw
    printf("u.area()=%.2f\n", printedArea(u));
    return 0;
}

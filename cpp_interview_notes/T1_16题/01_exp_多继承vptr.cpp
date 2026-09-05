// 01 补充实验: 多继承下的多 vptr —— 单继承 1 根, 多继承每条基类子对象各 1 根
// 预测题(先别跑): sizeof(D) = ?  (A)8  (B)16   &d 和 (B*)&d 的地址相等吗?
#include <cstdio>

struct Animal {                 // 含虚函数的基类 1
    virtual void speak() { printf("Animal::speak\n"); }
    virtual ~Animal() = default;
};
struct Robot {                  // 含虚函数的基类 2 (和 Animal 毫无关系)
    virtual void recharge() { printf("Robot::recharge\n"); }
    virtual ~Robot() = default;
};

// 单继承: 狗 = 动物 (1 条含虚基类)
struct Dog : Animal {
    void speak() override { printf("Woof\n"); }
};

// 多继承: 机器狗 = 动物 + 机器人 (2 条含虚基类!)
class RobotDog : public Animal, public Robot {
public:
    void speak() override { printf("RobotDog: Woof-01\n"); }
    void recharge() override { printf("RobotDog: charging...\n"); }
};

int main() {
    printf("--- 单继承: sizeof(Dog) = %zu (一根 vptr, 复用 Animal 的布局)\n\n", sizeof(Dog));

    RobotDog rd;
    printf("--- 多继承: sizeof(RobotDog) = %zu (两根 vptr!)\n", sizeof(RobotDog));

    // 两种基类指针指向同一个对象, 地址却不同 —— 编译器偷偷做了偏移
    Animal* pa = &rd;            // 指向对象开头 (Animal 子对象, 偏移 +0)
    Robot*  pb = &rd;            // 指向 Robot 子对象, 偏移 +8
    printf("  &rd      = %p\n", (void*)&rd);
    printf("  (Animal*)&rd = %p   ← 和对象地址相同\n", (void*)pa);
    printf("  (Robot*)&rd  = %p   ← 差了 8 字节 = 第二根 vptr 的位置\n\n", (void*)pb);

    // 两根 vptr 各自工作: 通过哪个基类指针调, 就查哪张表
    pa->speak();                 // 查 D-as-Animal 表 → RobotDog::speak
    pb->recharge();              // 查 D-as-Robot 表  → RobotDog::recharge

    // 对照: 不含虚函数的基类不占 vptr
    struct Tag {};                                  // 空基类, 无虚函数
    struct Mixed : Animal, Tag {};                  // Tag 靠空基类优化白住
    printf("对照: Animal + 无虚函数的 Tag, sizeof = %zu (Tag 没带来第二根 vptr)\n", sizeof(Mixed));
    return 0;
}

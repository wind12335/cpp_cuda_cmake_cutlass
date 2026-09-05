#include <cstdio>
#include <iostream>
#include <cstdint>            // uint8_t 在这里(错误2: 之前用了编译器内部名 __uint8_t)
using std::cout;

enum class Color { Red=128, Green=108, Blue=114 };                       // ✓ 你写的, 完全正确

// 错误1: short 是 C++ 关键字(整数类型名), 不能当枚举成员名 → 改成 Short(大写开头)
// 错误2: __uint8_t 是编译器内部名 → 改成 uint8_t + #include <cstdint>
enum class Human : uint8_t { Short = 4, Tall = 108, Pretty, Fat = 114, Thin = 116 };

int main() {
    Color c = Color::Red;
    printf("Color::Red 的底层值 = %d\n", (int)c);
    c = Color::Green;
    printf("Color::Green 的底层值 = %d\n", (int)c);
    c = Color::Blue;
    printf("Color::Blue 的底层值 = %d\n", (int)c);
    cout << "-----------------------------------------------------------------------------" << std::endl;

    Human h = Human::Short;                               // Short (关键字不能用作名字)
    printf("Human::Short 的底层值 = %d\n", (int)h);
    h = Human::Tall;
    printf("Human::Tall 的底层值 = %d\n", (int)h);
    h = Human::Pretty;
    printf("Human::Pretty 的底层值 = %d\n", (int)h);
    h = Human::Fat;
    printf("Human::Fat 的底层值 = %d\n", (int)h);
    h = Human::Thin;
    printf("Human::Thin 的底层值 = %d\n", (int)h);
    return 0;
}

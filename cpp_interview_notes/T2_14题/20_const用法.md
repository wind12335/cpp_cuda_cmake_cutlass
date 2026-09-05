# 20 const 的全部用法

## 面试口述版

按"修饰谁"分四组说：
① **修饰变量**：const int x —— 不可写，编译期检查；文件级 const 默认内部链接。
② **修饰指针**（考最细）：`const int* p`（指向常量，*p 不可写，p 可挪）、`int* const p`（指针本身常量，p 不可挪，*p 可写）、`const int* const p`（都不可）——口诀"左定值右定针"（const 在 * 左边定值，右边定针）。
③ **修饰成员函数**：`int get() const` —— 承诺不改成员（this 变成 const T*），const 对象只能调 const 成员函数；
需要改的少数场景（缓存、计数）用 **mutable** 豁免。
④ **修饰引用/形参**：`const T&` 是"只读借用"——不拷贝又不许改，函数传大对象的标准姿势；
顶层 const（指针本身）和底层 const（指向的值）的区分是重载和类型转换的基础。

## 高频追问

- **const 成员函数里返回成员引用，要不要 const？** 要：`const std::string& name() const`，否则外部绕过 const 限制改内部。
- **const 对象怎么初始化？** 必须在构造函数**初始化列表**里初始化，不能在函数体赋值（同理引用成员）。
- **const_cast 合法吗？** 改"真常量"（本来就定义为 const 的对象）是 UB；去掉"指针/引用上的底层 const"访问本可写的对象才合法。
- **constexpr 和 const 区别？** const = 运行期只读；constexpr = 编译期常量表达式，可用作数组维度/模板参数。

## 代码

见 `20_const.cpp`：三种指针 const、const 成员函数与 mutable、初始化列表、const 对象的限制。

```bash
g++ -std=c++17 20_const.cpp -o t20 && ./t20
```

## 记忆钩子

"左定值右定针；const 对象只走 const 门；mutable 是后门，初始化列表是产房。"

**人话版**：① `const` 在 `*` **左边**修饰"值"（`const int* p`：*p 不可写），在 `*` **右边**修饰
"指针本身"（`int* const p`：p 不可挪）；
② const 对象只能调用 const 成员函数（"const 门"），普通成员函数一律拒之门外；
③ **mutable** 成员是例外：即使在 const 函数里也能改（给缓存、计数器这类"不影响逻辑状态"的变量开后门）；
④ const/引用成员必须在**构造函数初始化列表**里初始化（"产房"——它们没有默认构造，不能先出生再赋值）。

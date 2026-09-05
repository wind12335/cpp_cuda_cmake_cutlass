# 18 RVO / NRVO：返回值优化

## 面试口述版

RVO（返回值优化）指**函数按值返回对象时，直接在调用方的目标位置上构造对象**，
跳过"局部构造→拷贝/移动出来→销毁局部"的整个过程——零拷贝零移动。
分两种：**RVO**（返回纯临时对象，如 `return Widget();`）C++17 起是**语言强制保证**，写不写都消除；
**NRVO**（返回具名局部变量，如 `Widget w; ...; return w;`）仍是编译器优化，主流编译器 -O1 以上基本都做，
但存在做不了的场景：返回的是函数参数、return 路径不止一个对象（分支返回不同变量）。
验证手段：`-fno-elide-constructors` 关掉消除，打印构造次数对比。
最重要的实践结论：**返回局部对象时直接 `return w;`，绝不写 `return std::move(w)`**——
move 会把"可消除的返回"变成"右值表达式"，RVO 直接失效，白多一次移动。

## 高频追问

- **RVO 和移动语义什么关系？** RVO 优先——能消除连移动都省了；消除不了才轮到移动兜底。优先级：消除 > 移动 > 拷贝。
- **返回容器呢？** 同样适用：`vector<int> make() { vector<int> v; ...; return v; }` 零拷贝，放心按值返回。
- **怎么让编译器别优化来验证？** `-fno-elide-constructors`（GCC/Clang），MSVC 用 `/Zc:nrvo-`。

## 代码

见 `18_rvo.cpp`：报账类数构造次数；分别演示 RVO、NRVO、分支返回、move 抑制 RVO。

```bash
g++ -std=c++17 18_rvo.cpp -o t18 && ./t18
g++ -std=c++17 -fno-elide-constructors 18_rvo.cpp -o t18_noelide && ./t18_noelide   # 对比
```

## 记忆钩子

"return 临时必消除，return 具名看编译器；想快就别手写 move。"

**人话版**：`return Widget();`（返回纯临时对象）→ C++17 保证零拷贝零移动，编译器必须做到；
`Widget w; return w;`（返回具名变量）→ 通常也被优化（NRVO），但不强制、看编译器。
最重要的实践：返回局部对象直接 `return w;`，**别写 `return std::move(w)`**——move 会把可消除的
返回变成右值表达式，RVO 失效，凭空多一次移动。

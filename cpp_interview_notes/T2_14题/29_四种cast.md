# 29 四种 cast 的区别

## 面试口述版

C 风格强转 `(T)x` 什么都能转、看不出意图，C++ 拆成四种，各管一段：
① **static_cast**：编译期可判定的"合理"转换——数值类型互转、void* 还原、继承体系内的上行/下行转换。
不检查运行期类型，**下行转错对象就是 UB**。
② **dynamic_cast**：继承体系下行的**运行期安全检查**——沿 RTTI 的类型信息查"实际对象是不是目标类型"，
指针版失败返回 nullptr，引用版失败抛 std::bad_cast；要求基类至少有一个虚函数（得有 RTTI）。
安全但慢（要查类型信息），多态下行转换才用它。
③ **const_cast**：只增删 const/volatile。合法用途是"接口没标 const 但实际不改"的老代码桥接；
对真常量对象去掉 const 再写是 UB。
④ **reinterpret_cast**：位模式重新解释，最暴力——指针类型互转、指针↔整数。
几乎总是平台相关、对齐/别名违规就是 UB；合法场景如把 buffer 当设备句柄传递、序列化底层。
准则：默认 static_cast，多态下行用 dynamic_cast，const/reinterpret 出现时必须能写出理由。

## 高频追问

- **dynamic_cast 怎么实现的？** 读对象 vptr 指向的类型信息（type_info），沿继承图匹配目标类型——所以开销比 static_cast 大得多，热路径别用。
- **static_cast 下行为什么危险？** 只按静态类型做偏移计算，不验证实际对象；指向基类但实际是别的派生类时，成员偏移全错。
- **什么时候其实该重构而不是 cast？** 需要 dynamic_cast 判断类型再分支 → 多半应该用虚函数/visitor 替代。

## 代码

见 `29_four_casts.cpp`：四种各一个正确示例 + 一个被注释的危险示例。

```bash
g++ -std=c++17 29_four_casts.cpp -o t29 && ./t29
```

## 记忆钩子

"static 管合理，dynamic 管安全，const 管修饰，reinterpret 管暴力。"

**人话版**：四种 cast 的分工——**static_cast**：编译期能判断的"合理"转换（数值互转、void* 还原、
继承体系内上行下行），不检查运行期类型；**dynamic_cast**：多态下行转换时**运行期真查类型**，
转不动给 nullptr/抛异常，安全但慢；**const_cast**：只增删 const/volatile 修饰；**reinterpret_cast**：
把位模式硬重新解释，最暴力，几乎总是平台相关。
**RTTI**（dynamic_cast 依赖的机制）= Runtime Type Information，运行期类型信息：编译器为每个
含虚函数的类生成的"我是谁"元数据（通常挂在虚表旁边），dynamic_cast 和 typeid 靠它工作。

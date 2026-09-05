# C06 智能指针与 RAII 【核心】

> 解决 C++ 最经典的难题："new 出来的内存到底谁来 delete？"
> 一句话答案：**交给对象来管**（RAII），而智能指针就是"专门管内存的对象"。

## §C06.1 RAII 思想

**作用**：把资源的生命周期绑定到对象的生命周期——构造时拿资源、析构时放资源。
由于 C++ 保证栈上对象离开作用域**必然**析构（哪怕中途 return/抛异常），资源就必然被释放。

```cpp
void f() {
    int* p = new int[100];
    if (error) return;         // ← 传统写法: 这里 return 前忘了 delete[100] 就泄漏
    delete[] p;
}

void g() {
    std::vector<int> v(100);   // RAII: 不管从哪条路离开 g(), v 都会自动释放
    if (error) return;         // ← 随便 return, 安全
}
```

【前置：T1-16 RAII + 实验】cudaMalloc/cudaFree 的 RAII 封装见 X01——本章思想在 GPU 编程里天天用。

## §C06.2 std::unique_ptr：独占所有权 【核心，默认选择】

**作用**：一根指针的"自动 delete 外衣"——禁止拷贝（独占）、可移动（转移）、零开销。

```cpp
#include <memory>
auto p = std::make_unique<int>(42);   // 推荐创建方式(make_ 系列更安全)
printf("%d\n", *p);                   // 用起来和普通指针一样: *p, p->
// auto q = p;                        // ✗ 编译错: 禁止拷贝
auto q = std::move(p);                // ✓ 转移所有权, 之后 p == nullptr
// q 离开作用域时自动 delete —— 忘写 delete 这件事从此不可能

// 管理数组要用 [] 版本
auto arr = std::make_unique<int[]>(100);
arr[0] = 1;
```

【坑】`unique_ptr<Base> p = make_unique<Derived>()` 要求 Base 析构是 **virtual**（unique_ptr 不像
shared_ptr 记得实际类型，前置：T1-02）。

## §C06.3 std::shared_ptr：共享所有权

**作用**：多个持有者共享一个对象，引用计数归零时最后一个负责销毁。

```cpp
auto a = std::make_shared<int>(42);   // 一次分配(对象+控制块), 强计数=1
auto b = a;                           // 拷贝: 强计数=2
printf("%ld %ld\n", a.use_count(), b.use_count());   // 2 2
b.reset();                            // b 放弃: 强计数=1
// a 离开作用域: 强计数=0 → 对象销毁
```

【机制】shared_ptr = 两根指针（对象 + 控制块），控制块装强/弱计数、deleter；
weak_ptr 是观察者（弱计数），`lock()` 活着换 shared、死了给空——
**手搓实现与五幕剧情见 T1-04，本章不重复**。

## §C06.4 std::weak_ptr：观察者（打破循环引用）

**作用**：只观察不拥有（不加强计数），用于"可能会失效的引用"。

```cpp
std::weak_ptr<int> w = a;             // 观察 a 管的对象, 强计数不变
if (auto sp = w.lock()) { /* 活着: 拿到临时 shared_ptr */ }
else { /* 已死: 安全得知, 不会碰悬垂内存 */ }
```

【前置：T1-06 循环引用】父子互指时，子→父那条边必须用 weak_ptr。

## §C06.5 选择标准（面试必背）

1. **默认 unique_ptr**——独占是常态，零开销；
2. 确需共享所有权才升级 shared_ptr（注意：**循环引用要用 weak_ptr 断开**）；
3. 观察但不保证存活 → weak_ptr；
4. 涉及 CUDA：**device 显存**没有 std 智能指针，自己写 RAII 封装（X01 手把手）；
   host pinned 内存同理——deleter 换成 `cudaFreeHost` 即可
   `std::shared_ptr<float> h(ptr, cudaFreeHost);`（这是 deleter 机制的实战用法，前置：T1-04）。

## 本篇实验

`labs/C06_smartptr.cpp`——unique/shared/weak 全套 + 自定义删除器管一个"模拟资源"。

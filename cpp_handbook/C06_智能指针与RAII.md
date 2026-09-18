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

### §C06.1.1 这段代码到底想说啥（确认你的理解）

你的理解**基本正确**，帮你再拧精确两格：

1. **裸 new/delete 的死穴不是"要手写 delete"，而是"每个离开函数的出口都得记得写"**：中途 return、抛异常、continue……出口越多越容易漏。上面 f() 只是漏写了一处 delete。
2. **vector 安全的根源不是"std 提供的类"这个身份，而是它的析构函数 + 一条语言铁律**：**栈上对象离开作用域时，析构函数必然被调用**（return 也好、抛异常也好，C++ 保证）。vector 的析构函数里写了"释放内存"，所以内存跟着对象一起走。
3. 所以 RAII 不是 vector 的专利——**任何类**只要"构造拿资源、析构放资源"就是 RAII。智能指针就是把这个模式做成的"专门管一根指针的迷你类"，你也可以为自己的资源写（CUDA 的 cudaMalloc/cudaFree 就是 X01 里亲手封装的例子）。

**人话版**：裸指针像"拿了钥匙的人从任何门离开都可能忘了还"；RAII 对象像"钥匙挂在工牌上，人一出门闸机自动收走"——不管你走哪个门。

**记忆钩子**："资源绑在对象上，对象出域必析构——释放这件事，交给必然发生的事件。"

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

### §C06.2.1 make_unique 是什么 + 三个常见误解澄清（实测）

**误解①："unique_ptr 被废弃了"——没有！** 你想到的是 **`std::auto_ptr`**（C++98 的老智能指针，
拷贝会偷偷转移所有权，坑太多 → C++11 弃用、**C++17 直接删除**）。它的替代品正是
unique_ptr——unique_ptr 是**现行标准的默认推荐**，活得好好的。auto_ptr 才是尸体。

**误解②："make_unique 像其他智能指针一样有控制块"——unique_ptr 根本没有控制块！**
先分清两个东西的角色：

- `std::unique_ptr<int>` 是**指针类型**（智能指针本体）
- `std::make_unique<int>(...)` 是**工厂函数**：一行完成"new + 包进 unique_ptr 返回"，
  它不是指针、不占内存，只是个便捷的"生产线"（C++14 加入，为了和 make_shared 配对）

内部结构实测（sizeof 见底）：

| | 内部装什么 | sizeof |
|---|---|---|
| `unique_ptr<int>` | **就一根裸指针**（deleter 是无状态的，零额外字节） | **8** 字节 |
| `shared_ptr<int>` | **两根指针**：对象指针 + 控制块指针（控制块里才有强/弱计数、deleter） | **16** 字节 |

你在 T1-04 手搓过的控制块（强计数+弱计数+deleter）是 **shared_ptr 专属**——因为"共享"
才需要数人头；unique_ptr 独占所有权，没有"数人头"这回事，所以**零开销**（和裸指针一样大）。

**误解③：`make_unique<int[]>(100)` 是"长度 1 填 100"？——是长度 100 的数组！**
和 `make_unique<int>(100)` 一字之差天壤之别（实测）：

```cpp
auto p   = std::make_unique<int>(100);    // new int(100):  一个 int, 值是 100
printf("%d", *p);                         // 打印 100

auto arr = std::make_unique<int[]>(100);  // new int[100]: 100 个 int 的数组
printf("%d %d", arr[0], arr[99]);         // 打印 0 0 (数组版会全部清零)
```

记忆法：**尖括号里有 `[]` = 数组**；圆括号里的 100，单数版是"值"、数组版是"长度"。

**为什么推荐 make 系而不是手写 new**：① 一行搞定不裸 new；② 异常安全
（C++17 前函数参数求值顺序不定，`f(unique_ptr<T>(new T), g())` 里 g() 抛异常就泄漏，
make_unique 版无此坑）；③ 和 make_shared 写法统一。

**人话版**：make_unique 是"免摘流水线"——你报个参数，它 new 好、包好、递给你 unique_ptr；
盒子里面**就一根指针**，没有控制块那种高级机关（那是隔壁 shared_ptr 的豪宅配置）。

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

### §C06.3.1 shared_ptr vs make_unique（这俩其实不是一对）+ use_count 实测

**先摆正对照关系**——它们不在同一个维度上：

| 名字 | 是什么 |
|---|---|
| `unique_ptr` / `shared_ptr` | 两种**指针类型**（独占 vs 共享，互为替代选项） |
| `make_unique` / `make_shared` | 两条**生产线**（分别造上面两种，互为配对） |

所以"shared_ptr 和 make_unique 的不同"= 问了一个跨维度的比较。真正的对比是：
**shared_ptr（共享，有控制块，拷贝自由）vs unique_ptr（独占，无控制块，禁拷贝）**。

**`.use_count()` 在做啥**：问控制块"现在有几个 shared_ptr 共有这个对象"（强引用计数）。
实测全流程：

```cpp
auto a = std::make_shared<int>(42);
a.use_count();              // 1 (只有 a)
{ auto b = a;               // 拷贝: 又一个人共有
  a.use_count(); b.use_count(); }   // 2 2 (同一对象、同一计数, 谁问都是 2)
a.use_count();              // 1 (b 出了作用域, 计数减回)
auto w = std::weak_ptr<int>(a);
a.use_count();              // 还是 1! (weak 只观察, 不加强计数)
// a 出作用域 → 0 → 对象销毁
```

**人话版**：use_count 就是控制块里的"在住人数登记簿"；weak_ptr 是"访客"，登记簿不为访客加数。

## §C06.4 std::weak_ptr：观察者（打破循环引用）

**作用**：只观察不拥有（不加强计数），用于"可能会失效的引用"。

```cpp
std::weak_ptr<int> w = a;             // 观察 a 管的对象, 强计数不变
if (auto sp = w.lock()) { /* 活着: 拿到临时 shared_ptr */ }
else { /* 已死: 安全得知, 不会碰悬垂内存 */ }
```

【前置：T1-06 循环引用】父子互指时，子→父那条边必须用 weak_ptr。

### §C06.4.1 weak_ptr 和前两者的区别 + 循环引用泄漏实验（实测）

三者一张表：

| | unique_ptr | shared_ptr | weak_ptr |
|---|---|---|---|
| 拥有对象？ | **独占** | 共有（数人头） | **不拥有**（纯观察） |
| 控制块 | ✗ | ✓ | 借 shared 的（只动**弱**计数） |
| 能直接 `*w` 访问？ | ✓ | ✓ | **✗**（必须先 `lock()`） |
| 存在的意义 | 默认选择 | 多个持有者 | ①观察"可能已死"的对象 ②**断开循环引用** |

**为什么 weak_ptr 必须先 lock()**：它不拥有对象，对象可能已死——直接解引用就是踩悬垂内存。
`w.lock()` 干的事："活着的话借我一个 shared_ptr（计数临时+1，保证我用期间不死）；死了给空"。

**循环引用为什么是灾难（实测完整复现）**——两个节点互相 shared：

```cpp
struct Node {
    std::string name;
    std::shared_ptr<Node> peer;      // ← 罪魁: 两边都是 shared
    static int alive;
    Node(std::string n) : name(std::move(n)) { ++alive; }
    ~Node() { --alive; }
};
int Node::alive = 0;
{
    auto n1 = std::make_shared<Node>("A");
    auto n2 = std::make_shared<Node>("B");
    n1->peer = n2;  n2->peer = n1;   // A 抓着 B, B 抓着 A
}   // 出作用域: n1/n2 这两个"外部把手"没了
    // 但 A 的计数还有 B 持着(=1), B 的计数还有 A 持着(=1)
printf("%d", Node::alive);           // 实测打印 2 —— 两个对象永不析构, 泄漏!

// 解法: 一边(或两边)改成 weak_ptr<Node> peer;
// 实测同款代码换 weak 后: alive = 0, 正确析构 ✓
```

**人话版**：shared 互指 = 两个人互相说"他欠我钱我不能走"——谁也不还钱（计数不归零）
房子永远占着。weak = 单方面"我认识他"——人走茶凉，不阻碍对方退房。

**记忆钩子**："shared 断循环，一端换 weak；用前先 lock()，死了拿空来。"

### §C06.4.2 lock() 的机制 + 弱计数为什么"看不见"（实测）

**lock() = 原子地"查活 + 借一个"**。它一步干两件事（中间不允许任何人插队）：
① 看强计数是否 > 0（对象还活着吗）；② 活着就**构造一个 shared_ptr**（强计数临时 +1）还给你。
死了就返回**空的 shared_ptr**。

```cpp
auto s = std::make_shared<int>(42);
std::weak_ptr<int> w = s;
s.use_count();               // 1
if (auto sp = w.lock()) {    // 借: 强计数变 2
    *sp;                     // 期间对象保证不死(我也持有它了)
}                            // sp 出作用域: 自动还, 强计数回 1
s.reset();                   // 强计数 0 → 对象销毁
w.lock();                    // 空 shared_ptr; w.expired() == true
```

**为什么必须这么绕（而不是直接 `*w`）**：weak_ptr 不拥有对象，你"看一眼"的瞬间对象可能
恰好死了（单线程里少见，多线程里随时发生）。lock() 把"确认活着"和"拿使用权"**合并成
一个原子动作**，借到之后你持有一个 shared_ptr，对象在你用完之前不可能死。

**弱计数为什么看不见**：`w.use_count()` 查的也是**强**计数（实测和 s.use_count() 永远相等）
——标准**没有**查询弱计数的接口。弱计数只在控制块内部记账，它唯一的职责：
**强计数归零 → 销毁对象；弱计数也归零 → 才销毁控制块本身**（账本还有读者，账本不能撕）。

**人话版**：weak 是不办门卡的访客；lock() = 到前台"原子地"问一句"人还在吗？在就给我
张临时门卡（shared_ptr）"——拿到卡期间这人保证不走；人走了就得到"查无此人"（空指针）。

**记忆钩子**："lock 查活又借卡，计数临时加一；弱计数藏在账本里，账本读者没了才撕。"

## §C06.5 直构 vs make_*：五种创建姿势对照（手搓前必看）

**直构** = 自己写 `new` 塞给构造函数；**make** = 工厂函数替你 new。weak_ptr 没有工厂，只能从 shared_ptr 生。

```cpp
// ── unique_ptr ──
std::unique_ptr<int> u1(new int(42));      // 直构(单值): 能用, 但裸露 new
auto u2 = std::make_unique<int>(42);       // 工厂(单值): 推荐, 一行不裸 new
std::unique_ptr<int[]> u3(new int[100]);   // 直构(数组): 类型要写全 int[]
auto u4 = std::make_unique<int[]>(100);    // 工厂(数组): 推荐, 自动清零

// ── shared_ptr ──
std::shared_ptr<int> s1(new int(42));      // 直构: 【2 次堆分配】对象一次+控制块一次
auto s2 = std::make_shared<int>(42);       // 工厂: 【1 次堆分配】对象+控制块打包成一块
std::shared_ptr<FILE> f(fopen(...),        // 自定义 deleter: 只能直构!
                        [](FILE* p){ if (p) fclose(p); });

// ── weak_ptr ──
std::weak_ptr<int> w = s2;                 // 没有 make_weak! 只能从 shared 生
if (auto sp = w.lock()) { /* 活着: sp 是借来的 shared_ptr */ }
if (w.expired()) { /* 死了 */ }             // 另一个常用查询: 还活着吗
```

**堆分配次数实测**（重载 operator new 计数，lab 里有完整版）：

| 写法 | 堆分配次数 | 说明 |
|---|---|---|
| `unique_ptr<T>(new T)` | 1 | 和 make_unique 一样 |
| `make_unique<T>()` | 1 | |
| `shared_ptr<T>(new T)` | **2** | 对象、控制块分开开两块 |
| `make_shared<T>()` | **1** | 对象+控制块**打包成一块**（更快、缓存更友好） |
| `weak_ptr w = s` | 0 | 纯观察，不开内存 |

**选型口诀**：
- 普通 new 出来的对象 → **make 优先**（少一次分配 + 异常安全 + 不见裸 new）
- 带**自定义 deleter** 的资源（FILE/cudaFreeHost/cudaFree）→ **只能直构**（make 不收 deleter）
- 数组 → `make_unique<T[]>`（C++14 起有）。**注意：`make_shared<T[]>` 任何标准版本都没有**
  （C++17/20/23 都没收录，GCC 会在 C++20 模式下仍报 `static assertion failed:
  make_shared<T[]> not supported`）——要共享数组：直构 `std::shared_ptr<char[]>(new char[10])`
  （`shared_ptr<T[]>` **类型**本身 C++17 就支持，会正确调 `delete[]`），或干脆用
  `vector<char>` / `string`（Rule of Zero，通常最佳）。

【延伸，手搓 shared_ptr 时会懂】make_shared 打包的副作用：只要还有 weak_ptr 活着，
**整块内存（含对象那部分）都不能还**（对象和控制在同一块，拆不开）；直构版则对象先还、
控制块等 weak 死光再还。你手搓时用"对象、控制块分开开"的直构版结构，更好写。

## §C06.6 选择标准（面试必背）

1. **默认 unique_ptr**——独占是常态，零开销；
2. 确需共享所有权才升级 shared_ptr（注意：**循环引用要用 weak_ptr 断开**）；
3. 观察但不保证存活 → weak_ptr；
4. 涉及 CUDA：**device 显存**没有 std 智能指针，自己写 RAII 封装（X01 手把手）；
   host pinned 内存同理——deleter 换成 `cudaFreeHost` 即可
   `std::shared_ptr<float> h(ptr, cudaFreeHost);`（这是 deleter 机制的实战用法，前置：T1-04）。

## 本篇实验

`labs/C06_smartptr.cpp`——unique/shared/weak 全套 + 自定义删除器管一个"模拟资源"。

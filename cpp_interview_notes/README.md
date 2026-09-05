# C++ 八股 38 题 —— 讲解 + 可运行代码

按面试优先级分三档，共 **38 个知识点**（T1×16 必考 / T2×14 高频 / T3×8 扫一眼）。
每题一个 `.md`（口述版答案 + 高频追问 + 记忆钩子）+ 一个 `.cpp`（可编译运行，直接观察结论）。

## 怎么用

1. **每天 2-3 题**：先读 md，跑 cpp 看输出，然后**合上笔记口述 60-90 秒**。说不出例子就改代码再跑。
2. 全部代码用这条命令编译运行（多线程题加 `-pthread`，文件头注释里写了每题的准确命令）：

```bash
g++ -std=c++17 -O2 文件名.cpp -o t && ./t
```

3. 用 `check_all.sh` 一键验证所有代码可编译：

```bash
bash check_all.sh
```

4. T3 是"面试前一晚扫一眼"档，速览 md + 一个合并 demo。

## 📖 阅读约定（新手必读）

- 每题的 **"记忆钩子"** 是一句压缩口诀，**只给已经理解的人当回忆开关用**——直接读它看不懂是正常的！
  口诀下面都配了 **"人话版"**，那是正经解释。学习顺序：读正文 → 跑代码 → 读人话版 → 最后用钩子自测
  （能从一句口诀还原出整段人话，才算真懂了）。
- 正文遇到任何没解释的术语，这是 bug——报文件名，我来补。

## ⚠️ 五步学习协议（每题 15-25 分钟，"扫一遍+跑一下"=白学）

md 是提词卡不是教材，cpp 是实验台不是演示品。深度来自你自己的操作：

1. **预测**：先只读 cpp 不跑，把每行预期输出**写在纸上**并写理由；
2. **对照**：跑代码。猜错的每一处 = 今天真正的知识漏洞，搞懂它而不是跳过它；
3. **破坏**：至少改一处代码再跑。改法从 md 的"高频追问"里找方向（删 virtual / 去 noexcept /
   改捕获方式 / 换容器…）。示例——第 01 题的三个实验：
   - 删掉 `virtual` → 输出从 `Woof` 变 `Animal speaks`，`sizeof` 从 8 变 1（多态消失 + 空类大小）；
   - 再加一个虚函数 → `sizeof` 仍是 8（虚函数进 vtable，对象里只有一根 vptr）；
   - `Animal::speak()` 显式限定 → 绕过查表的静态调用（派生类复用基类逻辑的标准写法）；
4. **追问自测**：遮住 md 的"高频追问"逐条口述；卡壳的**这时候才开阿秀**查那一小节，查完合上复述；
5. **口述 90 秒**：完整讲一遍，必须带实验证据（"我把 virtual 删了，输出变成…"）。

三关过完才算这题"毕业"，没毕业不进下一题。CUDA 和 OS/网络两套笔记同样适用此协议
（CUDA 把"破坏"换成改 kernel 配置/删关键字，OS/网络换成改 demo 参数观察数字变化）。

## 目录

### T1_16题 —— 每场面试必碰

| # | 题目 | 配套代码 |
|---|------|---------|
| 01 | 虚函数怎么实现多态 | `01_virtual_polymorphism.cpp` |
| 02 | 为什么基类析构要 virtual | `02_virtual_destructor.cpp` |
| 03 | 构造/析构里调虚函数会怎样 | `03_virtual_in_ctor_dtor.cpp` |
| 04 | shared_ptr 实现原理（含手写） | `04_shared_ptr_impl.cpp` |
| 05 | shared_ptr 线程安全（三层区分） | `05_shared_ptr_thread_safety.cpp` |
| 06 | 循环引用怎么解 | `06_circular_reference.cpp` |
| 07 | unique_ptr 为什么零开销 | `07_unique_ptr.cpp` |
| 08 | make_shared 好在哪 | `08_make_shared.cpp` |
| 09 | std::move 干了什么 | `09_std_move.cpp` |
| 10 | 移动构造什么时候触发 | `10_move_ctor_triggers.cpp` |
| 11 | vector 扩容机制 | `11_vector_growth.cpp` |
| 12 | push_back vs emplace_back | `12_push_vs_emplace.cpp` |
| 13 | map vs unordered_map | `13_map_vs_unordered.cpp` |
| 14 | new/delete vs malloc/free | `14_new_vs_malloc.cpp` |
| 15 | 堆 vs 栈 | `15_heap_vs_stack.cpp` |
| 16 | RAII | `16_raii.cpp` |

### T2_14题 —— 高频

| # | 题目 | 配套代码 |
|---|------|---------|
| 17 | 深拷贝 vs 浅拷贝（Rule of 3/5/0） | `17_deep_shallow_copy.cpp` |
| 18 | RVO / NRVO | `18_rvo.cpp`（附 -fno-elide-constructors 对照） |
| 19 | lambda 捕获的坑 | `19_lambda_capture.cpp` |
| 20 | const 全部用法 | `20_const.cpp` |
| 21 | static 全部用法 | `21_static.cpp` |
| 22 | atomic vs mutex | `22_atomic_vs_mutex.cpp` |
| 23 | 内存序 acquire/release | `23_memory_order.cpp` |
| 24 | 生产者消费者手写 | `24_producer_consumer.cpp`（面试手撕题） |
| 25 | false sharing | `25_false_sharing.cpp`（计时对比） |
| 26 | inline 与宏的区别 | `26_inline.cpp` |
| 27 | explicit | `27_explicit.cpp` |
| 28 | override / final | `28_override_final.cpp` |
| 29 | 四种 cast | `29_four_casts.cpp` |
| 30 | 空类大小 / vptr / EBO | `30_empty_class_size.cpp` |

### T3_8题 —— 面试前一晚扫一眼

`T3_八题速览.md`（31 菱形继承/虚继承、32 模板放头文件、33 this、34 friend、
35 decltype/auto、36 完美转发、37 函数指针 vs std::function、38 C++11 十特性）
+ `31_misc_demos.cpp`（合并验证）。

## 配套计划

- 起点节奏：每天 2-3 题 + 1 道 LeetCode（codetop 高频）。
- 二轮：只刷卡——每题 md 的"面试口述版"就是卡片正面，"记忆钩子"是提示，说不出就重跑代码。
- 这份清单是**全部**：阿秀/小林其余内容当字典查，不进主线。

## 说明

内容为按上述 38 题原创整理（讲解、代码、输出注释均为本项目编写），
面试口径参考了阿秀校招笔记/小林 coding 等公开题库的常见问法，未搬运原文。

# 14 new/delete vs malloc/free

## 面试口报版

四层区别，一层比一层深：
① **类型语义**：new = 分配内存 + 调用构造函数，delete = 调用析构 + 释放；malloc/free 只管裸内存。
② **类型安全**：new 返回正确类型指针不用强转，malloc 返回 void*；配对错误（new 出来交给 free）是未定义行为。
③ **可定制**：new 可以类级重载 operator new（池分配器、统计、对齐——**GPU 场景的 host pinned 内存封装就是这么做的**），
还可以 new(std::nothrow) 失败返回空而不是抛 bad_alloc。
④ **失败行为**：new 默认失败抛 std::bad_alloc（或触发 new_handler），malloc 返回 NULL。
配对纪律：new/delete、new[]/delete[]、malloc/free 各自配对，严禁交叉。

## 高频追问

- **new 表达式和 operator new 的区别？** new 表达式 = operator new（分配）+ 构造函数；delete 表达式 = 析构 + operator delete。可重载的是 operator new，不是 new 表达式的语义。
- **placement new？** 在已给定的内存上构造对象：`new (buf) T(...)`，配对要手动调 `p->~T()` —— 内存池/对象池的基本功。
- **new[] 为什么必须 delete[]？** new[] 会在块头记录元素个数，delete[] 按个数逐个析构；用 delete 只析构第一个。

## 代码

见 `14_new_vs_malloc.cpp`：构造/析构时机差异、placement new、nothrow 用法。

```bash
g++ -std=c++17 14_new_vs_malloc.cpp -o t14 && ./t14
```

## 记忆钩子

"malloc 给空地，new 盖好房再交钥匙；谁盖的谁拆，家具不乱配。"

**人话版**：malloc 只负责"给你一块指定大小的生内存"，不调构造函数——拿到手的是一块不能直接当
对象用的空地；new = 分配内存 **+ 调用构造函数**，拿到手的是能直接用的对象。
"谁盖的谁拆" = 释放必须严格配对：new 的用 delete、malloc 的用 free、new[] 的用 delete[]，
交叉混用（new 出来的交给 free）是未定义行为（"家具不乱配"）。

# 08 make_shared 比 new 好在哪

## 面试口述版

两个理由一个代价。
**理由一：一次分配。** `shared_ptr<T>(new T)` 是两次分配（一次对象、一次堆上控制块），
`make_shared<T>` 把对象和控制块放在**同一块内存**里一次分配 —— 少一次 malloc，对象和控制块相邻、缓存局部性更好。
**理由二：异常安全。** C++17 之前，`f(shared_ptr<T>(new T), g())` 这种表达式里，
new T 可能先执行、g() 随后抛异常、shared_ptr 构造还没开始 —— 泄漏；make_shared 把"分配对象"和"进入管理"
合成一个不可分割的函数调用，没有缝隙。（C++17 起求值顺序规则收紧，这个坑已修复，但答案仍要说全。）
**代价：** 对象和控制块同生共死 —— 只要还有 weak_ptr 观察着，整块内存（含对象本身那部分）都不能释放，
大对象 + 长命 weak_ptr 的场景会推迟内存回收；另外 make_shared 不能指定自定义删除器。

## 高频追问

- **make_unique 呢？** C++14 引入，理由是配对完整性和避免裸 new，没有 make_shared 的"一次分配"特性（unique 没有控制块）。
- **什么场景不用 make_shared？** 要自定义删除器、对象很大且 weak_ptr 生命周期长、对象构造函数是私有/protected 的（需要 pass_key 技巧）。

## 代码

见 `08_make_shared.cpp`：打印两种方式的对象地址与控制块（use_count 指针）地址，直观看到"同一块"。

```bash
g++ -std=c++17 08_make_shared.cpp -o t8 && ./t8
```

## 记忆钩子

"make_shared：省一次 malloc、堵一个异常窗口；代价是 weak 拖着整块内存。"

**人话版**：三个词翻译——
① **省一次 malloc**：`shared_ptr<T>(new T)` 要分配两次（对象一次、控制块一次），make_shared 合成一次；
② **堵一个异常窗口**：老 C++ 里 `f(shared_ptr<T>(new T), g())` 这种代码，可能"new 完了、还没装进
shared_ptr，g() 先抛异常"——对象泄漏。make_shared 把分配+接管合成一步，没有空档；
③ **weak 拖内存**：make_shared 把对象和控制块放同一块内存，只要还有 weak_ptr 在观察，
这一整块都不能还给系统（普通写法对象内存可以先还）。

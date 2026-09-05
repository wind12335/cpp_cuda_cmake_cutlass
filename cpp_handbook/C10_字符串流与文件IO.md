# C10 字符串流与文件 IO 【基础】

## §C10.1 iostream：cin / cout / cerr

**作用**：标准输入输出的 C++ 风格（类型安全，自动识别类型）。

```cpp
#include <iostream>
int x; double d;
std::cin >> x >> d;                    // 从键盘读(空格/换行分隔)
std::cout << "x=" << x << "\n";        // 输出, << 可以连着串
std::cerr << "错误信息\n";              // 标准错误(不带缓冲, 立即输出)

std::cout.precision(3);                // 控制有效数字
std::cout << 3.14159 << "\n";          // 3.14
```

【性能提示】`std::endl` = 输出 + **强制刷新缓冲**（慢），循环里用 `"\n"` 就好。
大工程里 GPU 日志常用 printf/cout 混着来，注意 cout 与 printf **别混用同一个流**（缓冲顺序会乱）。

## §C10.2 printf 风格（工程里更常见）

**作用**：格式化输出的 C 风格，CUDA 代码里最常用。

```cpp
printf("%d %zu %.3f %s %p\n", 42, sizeof(int), 3.14159, "str", (void*)&x);
// %d=int %u=unsigned %zu=size_t %lld=long long %f=double %.3f=三位小数
// %s=C字符串 %p=指针 %c=字符 %x=十六进制 %e=科学计数
```

【坑】格式符和实参类型不匹配 = 未定义行为（`%d` 打印 double 是经典错误）。

## §C10.3 stringstream：把字符串当流用

**作用**：解析字符串/拼装字符串的利器（读文本数据集、解析配置）。

```cpp
#include <sstream>
std::istringstream in("42 3.14 gpu");  // 把字符串当作输入流
int i; double d; std::string s;
in >> i >> d >> s;                      // i=42, d=3.14, s="gpu"

std::ostringstream out;                 // 拼装
out << "M=" << 4096 << " N=" << 4096;
std::string log = out.str();            // "M=4096 N=4096"
```

## §C10.4 fstream：文件读写

**作用**：读/写文件。

```cpp
#include <fstream>
// 写
std::ofstream out("result.txt");
out << "M=4096, TFLOPS=23.7\n";
out.close();                            // 或者靠析构自动关(RAII, 前置 C06)

// 读(逐行)
std::ifstream in("result.txt");
std::string line;
while (std::getline(in, line)) printf("%s\n", line.c_str());

// 读二进制(比如保存/加载 tensor)
std::vector<float> w(1024, 1.0f);
std::ofstream bin("w.bin", std::ios::binary);
bin.write(reinterpret_cast<char*>(w.data()), w.size() * sizeof(float));
bin.close();
std::ifstream inb("w.bin", std::ios::binary);
std::vector<float> w2(1024);
inb.read(reinterpret_cast<char*>(w2.data()), w2.size() * sizeof(float));
```

【CUDA 相关】保存/加载权重、记录 benchmark 日志都走这套；二进制块的 `reinterpret_cast<char*>`
是标准姿势（前置：T2-29 reinterpret_cast 的正当用途）。

## §C10.5 文件打开模式与状态检查

```cpp
std::ifstream in("no_such_file");
if (!in) { /* 打开失败! 记得检查 */ }
// 常用模式: std::ios::binary | std::ios::app(追加) | std::ios::in | std::ios::out
in.seekg(0, std::ios::end);             // 跳到文件末尾
size_t sz = in.tellg();                 // 得到文件大小 —— 读二进制前先量尺寸的套路
```

## 本篇实验

无独立 lab（示例都很短），写入/读回二进制的完整流程见上方 §C10.4，可直接复制。

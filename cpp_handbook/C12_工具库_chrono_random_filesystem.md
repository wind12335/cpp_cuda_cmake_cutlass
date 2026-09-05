# C12 工具库：chrono / random / filesystem / numeric 【工具】

## §C12.1 chrono：计时与时间 【CUDA benchmark 必备】

**作用**：高精度测量时间（你的每一个 benchmark 都要用它）。

```cpp
#include <chrono>
using clock_ = std::chrono::steady_clock;          // 单调时钟(不受系统改时间影响), 测耗时用它
auto t0 = clock_::now();
/* 被测代码 */
auto t1 = clock_::now();
double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();   // 毫秒
double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();    // 纳秒
```

三种时钟：`steady_clock`（单调，测耗时唯一正确选择）、`system_clock`（墙上时间，会被校时）、
`high_resolution_clock`（常是 steady 的别名，别依赖）。

【前置：CUDA G06】GPU 计时优先用 cudaEvent（GPU 时间线），chrono 用来测 host 端总耗时。

## §C12.2 random：真·随机数

**作用**：可复现的随机数生成（比 `rand()` 质量高一个量级）。

```cpp
#include <random>
std::mt19937 rng(42);                              // Mersenne Twister 引擎, 42=种子(可复现!)
std::uniform_real_distribution<float> uni(0.0f, 1.0f);    // [0,1) 均匀分布
std::normal_distribution<float> gauss(0.0f, 1.0f);        // 正态分布
for (int i = 0; i < 3; ++i) printf("%f ", uni(rng));      // 0.59 0.13 0.32...
// 初始化神经网络的权重: for(auto& x : w) x = gauss(rng);
```

【坑】每次跑程序要结果一样 → 固定种子；要不同 → `std::random_device{}()` 当种子。
【CUDA 相关】GPU 上大规模随机数用 cuRAND 库（G09 速查），host 端初始化种子仍用这套。

## §C12.3 filesystem：目录与文件（C++17）

**作用**：遍历目录、建目录、查文件大小——告别 system("ls")。

```cpp
#include <filesystem>
namespace fs = std::filesystem;
for (auto& entry : fs::directory_iterator("./data"))
    printf("%s %llu B\n", entry.path().c_str(), entry.file_size());
fs::create_directories("output/run1");             // 递归建目录
if (fs::exists("w.bin")) printf("存在, %zu 字节\n", (size_t)fs::file_size("w.bin"));
```

## §C12.4 numeric：数值算法

```cpp
#include <numeric>
std::vector<int> v = {1, 2, 3, 4};
std::iota(v.begin(), v.end(), 0);                  // 填 0,1,2,3(生成索引序列神器)
int sum = std::accumulate(v.begin(), v.end(), 0);          // 10
int dot = std::inner_product(v.begin(), v.end(), v.begin(), 0);  // 平方和=30(点积!)
std::vector<int> p(v.size());
std::partial_sum(v.begin(), v.end(), p.begin());   // 前缀和 1,3,6,10 (prefix sum/scan 的 CPU 版)
```

【CUDA 相关】`inner_product` = 点积 = GEMV；`partial_sum` = scan/prefix sum（GPU 上有专门的
并行 scan 算法，CUB 有现成实现）——CPU 版语义就是你的 GPU baseline。

## §C12.5 bit 与 utility 小工具（C++20/17）

```cpp
#include <bit>
int cnt = std::popcount(0b1011u);        // 3: 二进制里 1 的个数 (C++20)

#include <utility>
std::swap(a, b);                         // 交换
auto p = std::minmax({3, 1, 2});         // 同时拿 min 和 max: p.first=1, p.second=3
```

【坑】`std::popcount` 需 `<bit>` + **C++20**（编译加 `-std=c++20`，GCC≥10）；`std::minmax` 返回 pair，
注意别写 `auto&`（悬垂）。本篇 lab 对该段做了条件编译保护，`-std=c++17` 也能跑。

## §C12.6 functional：std::function 与可调用物

```cpp
#include <functional>
std::function<float(float)> f;           // 万能可调用物容器
f = [](float x) { return x * 2; };       // 装 lambda
f = [](float x) { return x + 1; };       // 换一个也行
printf("%.1f\n", f(3));                  // 4
```

【前置：T3-37】function 有类型擦除开销；性能敏感的模板接口（CUDA functor）不用它。

## 本篇实验

`labs/C12_util.cpp`——chrono 计时 random 权重初始化 numeric 四件套。

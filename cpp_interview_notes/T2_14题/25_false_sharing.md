# 25 false sharing：两线程写同 cache line 不同变量互相打脸

## 面试口述版

CPU 缓存以 **cache line（64 字节）**为单位在核间同步（MESI 协议）。
两个线程各自频繁写**逻辑上无关、物理上同一条 cache line**的变量（比如紧挨着的两个 int 计数器），
每次写都会让对方的缓存行失效，对方被迫从内存/远端缓存重新拉取——
数据没竞争，缓存行在疯狂竞争，性能可能掉几倍到几十倍。这就是 false sharing：**假的**共享，真的冲突。
解决：让热变量**各占一条 cache line**——`alignas(64)` 补齐、按 cache line 大小 padding、
或直接用 `std::hardware_destructive_interference_size`；
读多写少的共享数据无碍，问题专出在"多核高频写相邻变量"。

## 高频追问

- **怎么定位？** perf c2c（cache-to-canvas 专查这个）、VTune 的内存访问分析；症状是"逻辑上无锁、perf 里 HITM（命中他核改过的行）很高"。
- **和 GPU 的 shared memory bank conflict 是一回事吗？** 不是一回事但同属"存储粒度撞车"：bank conflict 是 warp 内线程撞 4 字节 bank，false sharing 是核间撞 64 字节缓存行，优化思路都是"错开存储位置"。
- **验证手段？** 配套代码：同一对计数器"紧挨着 vs 各自 alignas(64)"，多线程各写各的，计时对比（-O2 下差异通常明显，具体倍数看机器）。

## 代码

见 `25_false_sharing.cpp`：两个版本计时对比。

```bash
g++ -std=c++17 -O2 -pthread 25_false_sharing.cpp -o t25 && ./t25
```

## 记忆钩子

"64 字节一条船，两人各坐船头船尾照样一起晃；alignas(64) 一人一条船。"

**人话版**：CPU 在核间同步内存的最小单位是 **64 字节的缓存行**（cache line，"一条船"）——不是按
单个变量算的。两个线程写的哪怕是完全不同的变量，只要坐在同一条船上（同一条缓存行），每次写都会
让对方整条船作废重拉——数据无关，缓存打架。`alignas(64)` 让每个变量独占一条缓存行（一人一条船），
冲突消失。对照：GPU 的 shared memory bank conflict 是"粒度更小的同款撞车"（32 线程抢 4 字节 bank）。

# OS-07 CPU 缓存层级 + MESI

## 面试口述版

四级存储，延迟和带宽逐级跳水：**L1**（每核私有，指令/数据各 ~32-48KB，~4 cycle）→
**L2**（每核 ~1-2MB，~12 cycle）→ **L3**（全核共享 ~24-48MB，~40 cycle）→ **DRAM**（~200 cycle，带宽跌到 1/10）。
demo 用"工作集大小 vs 读带宽"扫描能直接看到悬崖：本机 236 GB/s（L1 内）→ 144（L2）→ 75→57（L3）→ 29 GB/s（内存）。
**MESI** 是核间缓存一致性协议，每个缓存行四态之一：**M**odified（本核改过，别核没有）、
**E**xclusive（只有本核有，干净）、**S**hared（多核都有只读副本）、**I**nvalid（无效）。
核 A 写一行 → 广播失效消息 → 其他核的副本 S→I，A 拿到 E→M——这保证"各核看到同一个值"，
**但不保证看到值的顺序**（顺序是内存序的事，联动 C++ 23 题），而失效-回填的往返正是
**false sharing 的代价来源**（联动 C++ 25 题）。跨核写同一行 ~百 ns 级往返，这就是"同行不相邻"纪律的由来。

## 高频追问

- **MESI 写一次的全过程？** 核 A 写缓存行 X：若 X 是 S 态，先发 Invalid 给所有持副本核，等 ACK（~几十到百 ns），升级 M 态再写；期间 A 的写被阻塞——高竞争行的延迟就堆积在这里。
- **为什么需要写缓冲/失效队列？** 等所有核 ACK 太慢，硬件用 store buffer + Invalidate Queue 异步化——这就是内存序重排的硬件来源，acquire/release 屏障管的就是它。
- **缓存行多大？** x86 主流 64B；`alignas(64)`、按行 padding、`std::hardware_destructive_interference_size` 都是工程对策。
- **GPU 联动**：GPU 的"L2 全卡共享 + 显存"结构类似，但**核间没有 MESI**——shared memory 一致性靠 `__syncthreads` 显式管，所以才有"写完要 barrier 才能读"的规则。

## 动手绑定

`demos/os7_cache_levels.c`：工作集从 16KB 扫到 512MB，四级悬崖直接画在终端里。

```bash
g++ -O2 -pthread demos/os7_cache_levels.c -o /tmp/os7 && /tmp/os7
# 实测(本机): 236 GB/s(L1) → 144(L2) → 75/57(L3) → 29(内存)
```

对照：`cpp_interview_notes/T2_14题/25_false_sharing.cpp`（MESI 副作用的代价实测）。

## 记忆钩子

"L1 三百六十五行，L3 三十天，内存一年（cycle 比例夸张记法）；MESI 管一致不管顺序，同行相写是灾难。"

**人话版**：前半句是延迟比例的夸张记忆法——L1 约 4 拍、L3 约 40 拍、内存约 200+ 拍，**每往下
一级慢约 5-10 倍**（"三百六十五行 vs 一年"就是让你记住"差得离谱"）。**MESI** 是多核缓存一致性
协议（缓存行四种状态：Modified/Exclusive/Shared/Invalid）：它保证各核看到**同一个值**，
但**不保证看到变化的先后顺序**（顺序是内存序的事）；而"两个核反复写同一条缓存行"会让行在核间
来回作废——这就是 false sharing（C++ 25 题）的灾难现场。

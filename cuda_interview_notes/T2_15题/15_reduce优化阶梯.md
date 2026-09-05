# 15 Reduce 完整优化阶梯（面试手写王牌题）

## 面试口述版

"优化一个 array reduction"是最高频的手写题，标准答案是一条**六级阶梯**，每级说清"瓶颈是什么、
改了什么、为什么快"：
① **naive 顺序**：一个线程串行加到底——完全无并行；
② **树形交错归约**：相邻元素两两折叠（stride 交错的树形），并行度全开但 shared 操作多、
后半段 warp 内 divergence（活跃线程收缩不齐）；
③ **消除 divergence**：把活跃线程改成**前缀排列**（0..active-1），warp 内不再分叉；
④ **warp shuffle 接管尾段**：block 内归约到每 warp 一个数后，最后 32→1 用 `__shfl_down_sync`
（联动 14 题），省掉最后几轮 shared+sync；
⑤ **每线程多元素**：先串行预累加 N 个（如 8 个），减少参与归约的线程数和 shared 流量
（memory-bound 场景这是大头，配 grid-stride 处理大数据）；
⑥ **向量化 + 载入合并**：float4 读取，对齐事务（联动 04 题）。
讲完阶梯再总结方法论："每一步都是先定位瓶颈（shared 流量/分歧/事务数）再对症下药"——
这套叙事比背代码值钱。你 `cuda_pybind_practice` 里那份《归约优化详解.md》就是现成的展开稿。

## 高频追问

- **什么时候加 atomic 版本？** 多 block 结果直接原子累加到 global（省一轮 launch），吞吐 vs 确定性的 trade-off：atomic 浮加顺序不定，结果 run-to-run 有细微差。
- **block 数怎么选？** 一般"每个 block 预累加一段"，block 数取 SM 数的整数倍（8×SM），保证各级都有活干。
- **和 NCCL allreduce 的联系？** 单卡内是"树形"；跨卡 ring allreduce 是带宽最优（2(N-1)/N × S）、树形是延迟最优——cost model 的取舍逻辑一致，正是你课题的日常。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读六级阶梯的落地版 | `interview/notes-v2.cu` Phase 1 全部 + `dot-product/dot_product.cu`（含 Vec4）+ `reduce/block_all_reduce.cu` |
| 你自己的笔记 | `cuda_pybind_practice/归约优化详解.md`（对照本文阶梯查漏） |
| 跑验证 | `./notes_v2_cute_sm89.bin` 看 BlockReduce / Dot / Dot-Vec4 三行 |
| 白板测试 | 不看任何参考，10 分钟写出"④ shuffle 版 block reduce"——这是面试验收线 |

## 记忆钩子

"顺序→交错树→前缀消分歧→shuffle 收尾→预累加减员→float4 进货：六级台阶，级级有理由。"

**人话版**：把"优化一个求和"的六步按顺序说清——①顺序累加：毫无并行；②交错树形：并行全开但
后半程 warp 内分叉严重；③**前缀排列**活跃线程（消分歧，第 06 题）；④最后 32→1 交给
**warp shuffle**（第 14 题，省掉多轮 shared 往返）；⑤每线程先**串行预累加**多个数，减少参与
归约的人数；⑥**float4 向量化**进货（第 04 题）。面试手写题按这六级讲，每级说清"上一级的瓶颈
是什么、这一级为什么解决它"。
